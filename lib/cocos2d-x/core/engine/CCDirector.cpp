/****************************************************************************
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2008-2010 Ricardo Quesada
Copyright (c) 2011      Zynga Inc.

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

// standard includes
#include <string>

// cocos2d includes
#include "engine/CCDirector.h"
#include "ccFPSImages.h"
#include "nodes/CCDrawingPrimitives.h"
#include "CCConfiguration.h"
#include "nodes/CCScene.h"
#include "cocoa/CCArray.h"
#include "CCScheduler.h"
#include "ccMacros.h"
#include "touch_dispatcher/CCTouchDispatcher.h"
#include "support/CCPointExtension.h"

#include "cocoa/CCAutoreleasePool.h"
#include "platform/CCTimer.h"
#include "nodes/CCLabelBMFont.h"
#include "nodes/CCLabelAtlas.h"
#include "actions/CCActionManager.h"
#include "CCConfiguration.h"
#include "touch_dispatcher/CCTouch.h"
#include "support/user_default/CCUserDefault.h"
#include "shaders/ccGLStateCache.h"
#include "shaders/CCShaderCache.h"
#include "kazmath/kazmath.h"
#include "kazmath/GL/matrix.h"
#include "support/CCProfiling.h"
#include "platform/CCImage.h"
#include "CCEGLView.h"
#include "CCConfiguration.h"

#include "base/MathDefs.h"
#include "CCInput.h"
#include "engine/CCEngineEvents.h"
#include "CCResourceCache.h"
#include "CCModule.h"

/**
 Position of the FPS
 
 Default: 0,0 (bottom-left corner)
 */
#ifndef CC_DIRECTOR_STATS_POSITION
#define CC_DIRECTOR_STATS_POSITION CCDirector::sharedDirector()->getVisibleOrigin()
#endif

using namespace std;

unsigned int g_uNumberOfDraws = 0;

NS_CC_BEGIN
// XXX it should be a Director ivar. Move it there once support for multiple directors is added

// singleton stuff
static WeakPtr<CCDirector> s_SharedDirector;

#define kDefaultFPS        60  // 60 frames per second
extern const char* cocos2dVersion(void);

// define in ccobject.cpp
void setCCOBJ_director_ref(CCDirector* ref);
CCDirector* CCDirector::sharedDirector(void)
{
    if (!s_SharedDirector)
    {
        s_SharedDirector = new CCDirector();
        
        setCCOBJ_director_ref(s_SharedDirector);
    }

    return s_SharedDirector;
}

CCDirector::CCDirector(void)
{
    CCLOG("alloc CCDirector %p", this);
}

extern "C" {
void imgui_init();
void imgui_draw();
}
bool CCDirector::init(void)
{
	setDefaultValues();
#if CC_TARGET_PLATFORM == CC_PLATFORM_WIN32 || CC_TARGET_PLATFORM == CC_PLATFORM_MAC
    imgui_init();
#endif
	m_bInvalid = (false);

    m_pNotificationNode = NULL;

    // projection delegate if "Custom" projection is used
    m_pProjectionDelegate = NULL;

    // FPS
	m_uTotalFrames = 0;
    m_pLastUpdate = new struct cc_timeval();

    // paused ?
    m_bPaused = false;
   
    // purge ?
    m_bPurgeDirecotorInNextLoop = false;

    m_obWinSizeInPoints = CCSizeZero;    

    m_pobOpenGLView = NULL;

    m_fContentScaleFactor = 1.0f;

	ModuleManager::addModule<CoreModule>();

	addSubSystem<CCScheduler>();
	addSubSystem<CCActionManager>();
	addSubSystem<CCTouchDispatcher>();
	addSubSystem<ResourceCache>();

    // create autorelease pool
    CCPoolManager::sharedPoolManager()->push();

	_scene = CCScene::create();
	_scene->onEnter();
	startAnimation();

    return true;
}
    
CCDirector::~CCDirector(void)
{
    CCLOG("deallocing CCDirector %p", this);
    
    CC_SAFE_RELEASE(m_pNotificationNode);

    // pop the autorelease pool
    CCPoolManager::sharedPoolManager()->pop();
    CCPoolManager::purgePoolManager();

    // delete m_pLastUpdate
    CC_SAFE_DELETE(m_pLastUpdate);
    
    _subSystems.clear();
}

void CCDirector::setDefaultValues(void)
{
	CCConfiguration *conf = CCConfiguration::sharedConfiguration();

	// default FPS
	double fps = conf->getNumber("cocos2d.x.fps", kDefaultFPS);
	m_dOldAnimationInterval = m_dAnimationInterval = 1.0 / fps;

	// GL projection
	const char *projection = conf->getCString("cocos2d.x.gl.projection", "3d");
	if( strcmp(projection, "3d") == 0 )
		m_eProjection = kCCDirectorProjection3D;
	else if (strcmp(projection, "2d") == 0)
		m_eProjection = kCCDirectorProjection2D;
	else if (strcmp(projection, "custom") == 0)
		m_eProjection = kCCDirectorProjectionCustom;
	else
		CCAssert(false, "Invalid projection value");

	// Default pixel format for PNG images with alpha
	const char *pixel_format = conf->getCString("cocos2d.x.texture.pixel_format_for_png", "rgba8888");
	if( strcmp(pixel_format, "rgba8888") == 0 )
		CCTexture2D::setDefaultAlphaPixelFormat(kCCTexture2DPixelFormat_RGBA8888);
	else if( strcmp(pixel_format, "rgba4444") == 0 )
		CCTexture2D::setDefaultAlphaPixelFormat(kCCTexture2DPixelFormat_RGBA4444);
	else if( strcmp(pixel_format, "rgba5551") == 0 )
		CCTexture2D::setDefaultAlphaPixelFormat(kCCTexture2DPixelFormat_RGB5A1);

	// PVR v2 has alpha premultiplied ?
	bool pvr_alpha_premultipled = conf->getBool("cocos2d.x.texture.pvrv2_has_alpha_premultiplied", false);
	CCTexture2D::PVRImagesHavePremultipliedAlpha(pvr_alpha_premultipled);
}

void CCDirector::setGLDefaultValues(void)
{
    // This method SHOULD be called only after openGLView_ was initialized
    CCAssert(m_pobOpenGLView, "opengl view should not be null");

    setAlphaBlending(true);
    // XXX: Fix me, should enable/disable depth test according the depth format as cocos2d-iphone did
    // [self setDepthTest: view_.depthFormat];
    setDepthTest(false);
    setProjection(m_eProjection);

    // set other opengl default values
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

// Draw the Scene
void CCDirector::drawScene(void)
{
    // calculate "global" dt
    calculateDeltaTime();

    //tick before glClear: issue #533
    if (! m_bPaused)
    {
		static UpdateEvent data;
		data[UpdateEvent::timeStep] = m_fDeltaTime;
		sendEvent(data);
    }
	g_uNumberOfDraws = 0;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    kmGLPushMatrix();

    // draw the scene
    if (_scene)
    {
        _scene->visit();
    }

    // draw the notifications node
    if (m_pNotificationNode)
    {
        m_pNotificationNode->visit();
    }
    
#if CC_TARGET_PLATFORM == CC_PLATFORM_WIN32 || CC_TARGET_PLATFORM == CC_PLATFORM_MAC
    imgui_draw();
#endif

    {
		static AfterDraw data;
		sendEvent(data);
    }

    kmGLPopMatrix();

    m_uTotalFrames++;

    // swap buffers
    if (m_pobOpenGLView)
    {
        m_pobOpenGLView->swapBuffers();
    }
}

void CCDirector::calculateDeltaTime(void)
{
    struct cc_timeval now;

    if (CCTime::gettimeofdayCocos2d(&now, NULL) != 0)
    {
        CCLOG("CCDirector: error in gettimeofday");
        m_fDeltaTime = 0;
        return;
    }

    // new delta time. Re-fixed issue #1277
    if (m_bNextDeltaTimeZero)
    {
        m_fDeltaTime = 0;
        m_bNextDeltaTimeZero = false;
    }
    else
    {
        m_fDeltaTime = (now.tv_sec - m_pLastUpdate->tv_sec) + (now.tv_usec - m_pLastUpdate->tv_usec) / 1000000.0f;
        m_fDeltaTime = Max(0.0f, m_fDeltaTime);
    }

#ifdef DEBUG
    // If we are debugging our code, prevent big delta time
    if(m_fDeltaTime > 0.2f)
    {
        m_fDeltaTime = 1 / 60.0f;
    }
#endif

    *m_pLastUpdate = now;
}
float CCDirector::getDeltaTime()
{
	return m_fDeltaTime;
}
void CCDirector::setOpenGLView(CCEGLView *pobOpenGLView)
{
    CCAssert(pobOpenGLView, "opengl view should not be null");

    if (m_pobOpenGLView != pobOpenGLView)
    {
		// Configuration. Gather GPU info
		CCConfiguration *conf = CCConfiguration::sharedConfiguration();
		conf->gatherGPUInfo();
		conf->dumpInfo();

        // EAGLView is not a CCObject
        if(m_pobOpenGLView)
            delete m_pobOpenGLView; // [openGLView_ release]
        m_pobOpenGLView = pobOpenGLView;

        // set size
        m_obWinSizeInPoints = m_pobOpenGLView->getFrameSize();
        
        if (m_pobOpenGLView)
        {
            setGLDefaultValues();
        }  
        
        CHECK_GL_ERROR_DEBUG();

		CCTouchDispatcher* td = getSubSystem<CCTouchDispatcher>();
        m_pobOpenGLView->setTouchDelegate(td);
        td->setDispatchEvents(true);
    }
}

void CCDirector::setViewport()
{
    if (m_pobOpenGLView)
    {
        m_pobOpenGLView->setViewPortInPoints(0, 0, m_obWinSizeInPoints.width, m_obWinSizeInPoints.height);
    }
}

void CCDirector::setNextDeltaTimeZero(bool bNextDeltaTimeZero)
{
    m_bNextDeltaTimeZero = bNextDeltaTimeZero;
}

void CCDirector::setProjection(ccDirectorProjection kProjection)
{
    CCSize size = m_obWinSizeInPoints;

    setViewport();

    switch (kProjection)
    {
    case kCCDirectorProjection2D:
        {
            kmGLMatrixMode(KM_GL_PROJECTION);
            kmGLLoadIdentity();
#if CC_TARGET_PLATFORM == CC_PLATFORM_WP8
            kmGLMultMatrix(CCEGLView::sharedOpenGLView()->getOrientationMatrix());
#endif
            kmMat4 orthoMatrix;
            kmMat4OrthographicProjection(&orthoMatrix, 0, size.width, 0, size.height, -1024, 1024 );
            kmGLMultMatrix(&orthoMatrix);
            kmGLMatrixMode(KM_GL_MODELVIEW);
            kmGLLoadIdentity();
        }
        break;

    case kCCDirectorProjection3D:
        {
            float zeye = this->getZEye();

            kmMat4 matrixPerspective, matrixLookup;

            kmGLMatrixMode(KM_GL_PROJECTION);
            kmGLLoadIdentity();
            
#if CC_TARGET_PLATFORM == CC_PLATFORM_WP8
            //if needed, we need to add a rotation for Landscape orientations on Windows Phone 8 since it is always in Portrait Mode
            kmGLMultMatrix(CCEGLView::sharedOpenGLView()->getOrientationMatrix());
#endif
            // issue #1334
            kmMat4PerspectiveProjection( &matrixPerspective, 60, (GLfloat)size.width/size.height, 0.1f, zeye*2);
            // kmMat4PerspectiveProjection( &matrixPerspective, 60, (GLfloat)size.width/size.height, 0.1f, 1500);

            kmGLMultMatrix(&matrixPerspective);

            kmGLMatrixMode(KM_GL_MODELVIEW);
            kmGLLoadIdentity();
            kmVec3 eye, center, up;
            kmVec3Fill( &eye, size.width/2, size.height/2, zeye );
            kmVec3Fill( &center, size.width/2, size.height/2, 0.0f );
            kmVec3Fill( &up, 0.0f, 1.0f, 0.0f);
            kmMat4LookAt(&matrixLookup, &eye, &center, &up);
            kmGLMultMatrix(&matrixLookup);
        }
        break;
            
    case kCCDirectorProjectionCustom:
        if (m_pProjectionDelegate)
        {
            m_pProjectionDelegate->updateProjection();
        }
        break;
            
    default:
        CCLOG("CCDirector: unrecognized projection");
        break;
    }

    m_eProjection = kProjection;
    ccSetProjectionMatrixDirty();
}

void CCDirector::purgeCachedData(void)
{
    if (s_SharedDirector->getOpenGLView())
    {
		getSubSystem<ResourceCache>()->removeUnused();
    }
}

float CCDirector::getZEye(void)
{
    return (m_obWinSizeInPoints.height / 1.1566f);
}

void CCDirector::setAlphaBlending(bool bOn)
{
    if (bOn)
    {
        ccGLBlendFunc(CC_BLEND_SRC, CC_BLEND_DST);
    }
    else
    {
        ccGLBlendFunc(GL_ONE, GL_ZERO);
    }

    CHECK_GL_ERROR_DEBUG();
}

void CCDirector::reshapeProjection(const CCSize& newWindowSize)
{
	CC_UNUSED_PARAM(newWindowSize);
	if (m_pobOpenGLView)
	{
		m_obWinSizeInPoints = CCSizeMake(newWindowSize.width * m_fContentScaleFactor,
			newWindowSize.height * m_fContentScaleFactor);
		setProjection(m_eProjection);       
	}

}
void CCDirector::setDepthTest(bool bOn)
{
    if (bOn)
    {
        glClearDepth(1.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
//        glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
    }
    CHECK_GL_ERROR_DEBUG();
}

static void
GLToClipTransform(kmMat4 *transformOut)
{
	kmMat4 projection;
	kmGLGetMatrix(KM_GL_PROJECTION, &projection);

#if CC_TARGET_PLATFORM == CC_PLATFORM_WP8
    //if needed, we need to undo the rotation for Landscape orientation in order to get the correct positions
	kmMat4Multiply(&projection, CCEGLView::sharedOpenGLView()->getReverseOrientationMatrix(), &projection);
#endif

	kmMat4 modelview;
	kmGLGetMatrix(KM_GL_MODELVIEW, &modelview);
	
	kmMat4Multiply(transformOut, &projection, &modelview);
}

CCPoint CCDirector::convertToGL(const CCPoint& uiPoint)
{
    kmMat4 transform;
	GLToClipTransform(&transform);
	
	kmMat4 transformInv;
	kmMat4Inverse(&transformInv, &transform);
	
	// Calculate z=0 using -> transform*[0, 0, 0, 1]/w
	kmScalar zClip = transform.mat[14]/transform.mat[15];
	
    CCSize glSize = m_pobOpenGLView->getFrameSize();
	kmVec3 clipCoord = {2.0f*uiPoint.x/glSize.width - 1.0f, 1.0f - 2.0f*uiPoint.y/glSize.height, zClip};
	
	kmVec3 glCoord;
	kmVec3TransformCoord(&glCoord, &clipCoord, &transformInv);
	
	return ccp(glCoord.x, glCoord.y);
}

CCPoint CCDirector::convertToUI(const CCPoint& glPoint)
{
    kmMat4 transform;
	GLToClipTransform(&transform);
    
	kmVec3 clipCoord;
	// Need to calculate the zero depth from the transform.
	kmVec3 glCoord = {glPoint.x, glPoint.y, 0.0};
	kmVec3TransformCoord(&clipCoord, &glCoord, &transform);
	
	CCSize glSize = m_pobOpenGLView->getFrameSize();
	return ccp(glSize.width*(clipCoord.x*0.5 + 0.5), glSize.height*(-clipCoord.y*0.5 + 0.5));
}

CCSize CCDirector::getWinSize(void)
{
    return m_obWinSizeInPoints;
}

CCSize CCDirector::getWinSizeInPixels()
{
    return CCSizeMake(m_obWinSizeInPoints.width * m_fContentScaleFactor, m_obWinSizeInPoints.height * m_fContentScaleFactor);
}

CCSize CCDirector::getVisibleSize()
{
	return m_pobOpenGLView->getFrameSize();
}

CCPoint CCDirector::getVisibleOrigin()
{
	return CCPointZero;
}

void CCDirector::end()
{
    m_bPurgeDirecotorInNextLoop = true;
}

void CCDirector::purgeDirector()
{
    // cleanup scheduler
	getSubSystem<CCScheduler>()->unscheduleAll();
    
    // don't release the event handlers
    // They are needed in case the director is run again
    getSubSystem<CCTouchDispatcher>()->removeAllDelegates();

	_scene.Reset();

    stopAnimation();

    // purge all managed caches
    ccDrawFree();
    CCShaderCache::purgeSharedShaderCache();
    CCConfiguration::purgeConfiguration();

    // cocos2d-x specific data structures
    CCUserDefault::purgeSharedUserDefault();

    ccGLInvalidateStateCache();
    
    CHECK_GL_ERROR_DEBUG();
    
    // OpenGL view
    m_pobOpenGLView->end();
    m_pobOpenGLView = NULL;

	s_SharedDirector.Reset();
    setCCOBJ_director_ref(nullptr);
}


void CCDirector::pause(void)
{
    if (m_bPaused)
    {
        return;
    }

    m_dOldAnimationInterval = m_dAnimationInterval;

    // when paused, don't consume CPU
    setAnimationInterval(1 / 4.0);
    m_bPaused = true;
}

void CCDirector::resume(void)
{
    if (! m_bPaused)
    {
        return;
    }

    setAnimationInterval(m_dOldAnimationInterval);

    if (CCTime::gettimeofdayCocos2d(m_pLastUpdate, NULL) != 0)
    {
        CCLOG("CCDirector: Error in gettimeofday");
    }

    m_bPaused = false;
	m_fDeltaTime = 0;
}

// returns the FPS image data pointer and len
void CCDirector::getFPSImageData(unsigned char** datapointer, unsigned int* length)
{
    // XXX fixed me if it should be used 
    *datapointer = cc_fps_images_png;
	*length = cc_fps_images_len();
}

float CCDirector::getContentScaleFactor(void)
{
    return m_fContentScaleFactor;
}

void CCDirector::setContentScaleFactor(float scaleFactor)
{
    if (scaleFactor != m_fContentScaleFactor)
    {
        m_fContentScaleFactor = scaleFactor;
    }
}

CCNode* CCDirector::getNotificationNode() 
{ 
    return m_pNotificationNode; 
}

void CCDirector::setNotificationNode(CCNode *node)
{
    CC_SAFE_RELEASE(m_pNotificationNode);
    m_pNotificationNode = node;
    CC_SAFE_RETAIN(m_pNotificationNode);
}

CCDirectorDelegate* CCDirector::getDelegate() const
{
    return m_pProjectionDelegate;
}

void CCDirector::setDelegate(CCDirectorDelegate* pDelegate)
{
    m_pProjectionDelegate = pDelegate;
}

/***************************************************
* implementation of DisplayLinkDirector
**************************************************/

// should we implement 4 types of director ??
// I think DisplayLinkDirector is enough
// so we now only support DisplayLinkDirector
void CCDirector::startAnimation(void)
{
    if (CCTime::gettimeofdayCocos2d(m_pLastUpdate, NULL) != 0)
    {
        CCLOG("cocos2d: DisplayLinkDirector: Error on gettimeofday");
    }

    m_bInvalid = false;
}

void CCDirector::mainLoop(void)
{
    if (m_bPurgeDirecotorInNextLoop)
    {
        m_bPurgeDirecotorInNextLoop = false;
        purgeDirector();
    }
    else if (! m_bInvalid)
     {
         drawScene();
     
         // release the objects
         CCPoolManager::sharedPoolManager()->pop();        
     }
}

void CCDirector::stopAnimation(void)
{
    m_bInvalid = true;
}

void CCDirector::setAnimationInterval(double dValue)
{
    m_dAnimationInterval = dValue;
    if (! m_bInvalid)
    {
        stopAnimation();
        startAnimation();
    }    
}


int CCDirector::run(CCApplication* app)
{
	/*
	std::string args;
	for (int i = 1; i < argc; ++i)
	{
	args.append((const char*)argv[i]);
	args.append(" ");
	}

	parseArguments(args);
	*/
	CCEGLView* e = CCEGLView::sharedOpenGLView();
	e->createWithSize(640, 960);

	this->init();
	this->setOpenGLView(e);

	if (!app->applicationDidFinishLaunching())
		return 1;

	while (isRunning())
		runFrame();

	return 0;
}

bool CCDirector::isRunning()
{
	return _running;
}

void CCDirector::runFrame()
{
	CCDirector::sharedDirector()->mainLoop();

	SubSystem::get<Input>()->update();
	//_input->update();

	timeLimit();
}

void CCDirector::timeLimit()
{
	long long targetMax = 1000000LL / _fps;
	long long elapsed = 0;

	for (;;)
	{
		elapsed = _frameTimer.elapsed();
		if (elapsed >= targetMax)
			break;

		// Sleep if 1 ms or more off the frame limiting goal
		if (targetMax - elapsed >= 1000LL)
		{
			unsigned sleepTime = (unsigned)((targetMax - elapsed) / 1000LL);
			CCTime::sleep(sleepTime);
		}
	}
	_frameTimer.reset();
}

void CCDirector::setFps(int fps)
{
	_fps = fps;
}

NS_CC_END


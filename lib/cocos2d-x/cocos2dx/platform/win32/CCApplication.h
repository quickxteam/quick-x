#ifndef __CC_APPLICATION_WIN32_H__
#define __CC_APPLICATION_WIN32_H__

#include "CCStdC.h"
#include "platform/CCApplicationProtocol.h"
#include <string>

NS_CC_BEGIN

class CCRect;

class CC_DLL CCApplication : public CCApplicationProtocol
{
public:
    CCApplication();
    virtual ~CCApplication();

    /**
    @brief    Run the message loop.
    */
    int run();

    /**
    @brief    Get current applicaiton instance.
    @return Current application instance pointer.
    */
    static CCApplication* sharedApplication();

    /* override functions */
    virtual void setAnimationInterval(double interval);
    virtual ccLanguageType getCurrentLanguage();
    
    /**
     @brief Get target platform
     */
    virtual TargetPlatform getTargetPlatform();

protected:
    HINSTANCE           m_hInstance;
    HACCEL              m_hAccelTable;
    LARGE_INTEGER       m_nAnimationInterval;

    static CCApplication * sm_pSharedApplication;
};

NS_CC_END

#endif    // __CC_APPLICATION_WIN32_H__

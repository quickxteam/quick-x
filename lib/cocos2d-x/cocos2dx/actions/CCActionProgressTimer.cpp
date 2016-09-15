/****************************************************************************
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (C) 2010      Lam Pham
 
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
#include "CCActionProgressTimer.h"
#include "misc_nodes/CCProgressTimer.h"

NS_CC_BEGIN

#define kProgressTimerCast CCProgressTimer*

// implementation of CCProgressTo

CCProgressTo* CCProgressTo::create(float duration, float fPercent)
{
    CCProgressTo *pProgressTo = new CCProgressTo();
    pProgressTo->initWithDuration(duration, fPercent);
    pProgressTo->autorelease();

    return pProgressTo;
}

bool CCProgressTo::initWithDuration(float duration, float fPercent)
{
    if (CCActionInterval::initWithDuration(duration))
    {
        m_fTo = fPercent;

        return true;
    }

    return false;
}

void CCProgressTo::paste(CCObject* o)
{
	Super::paste(o);
	O->initWithDuration(m_fDuration, m_fTo);
}

void CCProgressTo::startWithTarget(CCNode *pTarget)
{
    CCActionInterval::startWithTarget(pTarget);
    m_fFrom = ((kProgressTimerCast)(pTarget))->getPercentage();

    // XXX: Is this correct ?
    // Adding it to support CCRepeat
    if (m_fFrom == 100)
    {
        m_fFrom = 0;
    }
}

void CCProgressTo::update(float time)
{
    ((kProgressTimerCast)(m_pTarget))->setPercentage(m_fFrom + (m_fTo - m_fFrom) * time);
}

// implementation of CCProgressFromTo

CCProgressFromTo* CCProgressFromTo::create(float duration, float fFromPercentage, float fToPercentage)
{
    CCProgressFromTo *pProgressFromTo = new CCProgressFromTo();
    pProgressFromTo->initWithDuration(duration, fFromPercentage, fToPercentage);
    pProgressFromTo->autorelease();

    return pProgressFromTo;
}

bool CCProgressFromTo::initWithDuration(float duration, float fFromPercentage, float fToPercentage)
{
    if (CCActionInterval::initWithDuration(duration))
    {
        m_fTo = fToPercentage;
        m_fFrom = fFromPercentage;

        return true;
    }

    return false;
}

void CCProgressFromTo::paste(CCObject* o)
{
	Super::paste(o);
	O->initWithDuration(m_fDuration, m_fFrom, m_fTo);
}

CCActionInterval* CCProgressFromTo::reverse(void)
{
    return CCProgressFromTo::create(m_fDuration, m_fTo, m_fFrom);
}

void CCProgressFromTo::startWithTarget(CCNode *pTarget)
{
    CCActionInterval::startWithTarget(pTarget);
}

void CCProgressFromTo::update(float time)
{
    ((kProgressTimerCast)(m_pTarget))->setPercentage(m_fFrom + (m_fTo - m_fFrom) * time);
}

NS_CC_END



#pragma once

#include "engine/CCSubSystem.h"



NS_CC_BEGIN;


class Input : public SubSystem
{
	CCOBJECT(Input, SubSystem)
public:

	void onKeypadBack();
	void onKeypadMenu();

	void onEnterForeground();
	void onEnterBackground();
};

NS_CC_END;
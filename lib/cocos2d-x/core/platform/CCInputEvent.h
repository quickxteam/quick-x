
CC_EVENT_DEFINE(TouchBegin)
CC_PARAM(touchID)
CC_PARAM(x)
CC_PARAM(y)
CC_PARAM(pressure)
CC_EVENT_END()

CC_EVENT_DEFINE(TouchEnd)
CC_PARAM(touchID)
CC_PARAM(x)
CC_PARAM(y)
CC_EVENT_END()

CC_EVENT_DEFINE(TouchMove)
CC_PARAM(touchID)
CC_PARAM(x)
CC_PARAM(y)
CC_PARAM(dx)
CC_PARAM(dy)
CC_PARAM(pressure)
CC_EVENT_END()

CC_EVENT_DEFINE(TextInput)
CC_PARAM(text)
CC_EVENT_END()

CC_EVENT_DEFINE(TextEditing)
CC_PARAM(composition)
CC_PARAM(cursor)
CC_PARAM(selectionLength)
CC_EVENT_END()

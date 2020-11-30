﻿#ifndef CONTROLS_H
#define CONTROLS_H

#include "core/control.h"
#include "imagecontrol.h"
#include "videocontrol.h"
#include "strokecontrol.h"
#include "webcontrol.h"
#include "pptxcontrol.h"
#include "qmlcontrol.h"
#include "docxcontrol.h"
#include "textcontrol.h"
#include "wordscontrol.h"
#include "inputtextcontrol.h"

#include <qexport.h>

REGISTER_CONTROL(ImageControl, "image,bmp,gif,jpg,jpeg,png,svg")

#ifdef QT_DEBUG
REGISTER_CONTROL(StrokeControl, "glstroke")
#endif
REGISTER_CONTROL(WebControl, "htm,html,http,https,chrome,swf")
#ifdef QT_DEBUG
REGISTER_CONTROL(QmlControl, "qml")
#endif
REGISTER_CONTROL(TextControl, "text,txt,js,cpp,h,qss,css")
REGISTER_CONTROL(WordsControl, "words")

#ifdef QT_DEBUG
REGISTER_CONTROL(InputTextControl, "inputtext")
#endif

#ifdef WIN32
REGISTER_CONTROL(PptxControl, "ppt,pptx")
# ifdef QT_DEBUG
REGISTER_CONTROL(DocxControl, "doc,docx")
# endif
#endif


#endif // CONTROLS_H

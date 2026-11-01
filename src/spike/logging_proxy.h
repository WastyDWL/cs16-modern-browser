#pragma once
#include "../common/valve_html.h"

IHTMLChromeController* make_logging_controller(IHTMLChromeController* original);
IHTMLResponses_HL1* make_log_responses(IHTMLResponses_HL1* t);
IHTMLSerializer* make_log_serializer(IHTMLSerializer* s);

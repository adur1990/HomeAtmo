static const char *HTTP_HEADER =
"HTTP/1.1 200 OK\n"
"Content-type:text/html\n"
"Connection: close\n";

static const char *HTML_HEADER =
"<!DOCTYPE html>\n"
"<html>\n"
    "<head>\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "<link rel=\"icon\" href=\"data:,\">";

static const char *CSS =
        "<style>\n"
            "html {\n"
                "font-family: Helvetica;\n"
                "display: inline-block;\n"
                "margin: 0px auto;\n"
                "text-align: center;\n"
            "}\n"
            ".button {\n"
                "background-color: #4CAF50;\n"
                "border: none;\n"
                "color: white;\n"
                "padding: 16px 40px;\n"
                "text-decoration: none;\n"
                "font-size: 30px;\n"
                "margin: 2px;\n"
                "cursor: pointer;\n"
            "}\n"
            ".button2 {\n"
                "background-color: #555555;\n"
            "}\n"
        "</style>\n"
    "</head>";

static const char *BODY =
    "<body>\n"
        "<h1>ESP32 Web Server</h1>\n"
        "<p><a href=\"/req\"><button class=\"button\">REQUEST</button></a></p>\n"
        "<p><a href=\"/\"><button class=\"button2\">HOME</button></a></p>\n"
    "</body>\n"
"</html>\n";

#include "defines.h"

#include <cstdio>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

void message_to_file(const char* text)
{
    if (text) {
        std::fprintf(stderr, "%s\n", text);
    }
}

void Step(const char* text)
{
	if (!text)
		return;
	std::string text2(text);
	static std::ofstream file("c:\\temp\\stdout.txt");
	if(file)
		file << text2 << std::endl;
}
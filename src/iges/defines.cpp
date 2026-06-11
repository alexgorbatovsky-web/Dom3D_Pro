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

void Step(char* text)
{
	if (!text)
		return;
	std::string text2(text);
	std::ofstream file;
	const char* namefile = "c:\\temp\\stdout.txt";
	file.open(namefile, std::ofstream::out | std::ofstream::app);
	file << text2 << std::endl;
	file.close();
}
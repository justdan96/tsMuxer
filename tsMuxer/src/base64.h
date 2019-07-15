#ifndef __BASE_64_H
#define __BASE_64_H

#include <string>
#include <types/types.h>

class Base64 {
public:
	static std::string encode(uint8_t const* buffer, unsigned int len);
	static void decode(std::string const& encoded_string, char* buff, int bufLen, int& decodedLen);
};

#endif

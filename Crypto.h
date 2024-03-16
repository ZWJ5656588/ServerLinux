#pragma once
#include "Socket.h"

class Crypto
{
public:
	static Buffer MD5(const Buffer& text);
};


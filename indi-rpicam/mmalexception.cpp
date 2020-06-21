#include <iostream>
#include "mmalexception.h"

MMALException::MMALException(const char *text) : std::runtime_error(text)
{
}

void  MMALException::throw_if(bool status, const char *text)
{
    if(status) {
        throw MMALException(text);
    }
}

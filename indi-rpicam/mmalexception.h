#ifndef MMALEXCEPTION_H
#define MMALEXCEPTION_H
#include <stdexcept>
class MMALException : public std::runtime_error
{
public:
    MMALException(const char *text);
    static void throw_if(bool status, const char *text);
};
#endif // MMALEXCEPTION_H

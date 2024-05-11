#include <string>

#include "DLXJITException.h"

using namespace std;

DLXJITException::DLXJITException()
	: exception()
{
}

DLXJITException::DLXJITException(std::string message)
	: message(message)
{
}

DLXJITException::DLXJITException(exception& cause)
	: exception(cause)
{
}

const char* DLXJITException::what() const throw() {
    return message.c_str();
}


DLXJITException::~DLXJITException()
{
}

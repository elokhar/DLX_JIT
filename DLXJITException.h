#pragma once
#include <exception>

class DLXJITException : public std::exception
{
    std::string message;
public:
	DLXJITException();
	DLXJITException(std::string message);
	DLXJITException(std::exception& cause);
        
        const char* what() const throw() override;


	~DLXJITException() throw() override;
};
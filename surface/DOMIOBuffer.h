#ifndef DOMIOBUFFER_H_INCLUDED
#define DOMIOBUFFER_H_INCLUDED

#include <stdexcept>
#include <streambuf>
#include <string>

#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>

#include <iostream>

class DOMIOBuffer : public std::basic_streambuf<char, std::char_traits<char> >{
public:
	typedef char char_type;
	typedef int int_type;
private:
	int fd;
	unsigned int minReadSize;
	char_type* readBuffer;
	bool readEOF;
	std::string devicePath;
	
	enum rw{READ,WRITE};
	
	void waitReady(rw direction){
		fd_set set;
		fd_set* readset=NULL;
		fd_set* writeset=NULL;
		int maxfd = fd+1;
		FD_ZERO(&set);
		FD_SET(fd,&set);
		if(direction==READ)
			readset=&set;
		else
			writeset=&set;
		while(true){
			int result=select(maxfd,readset,writeset,NULL,NULL);
			/*if(result==1) //ready!
				return;
			if(errno==EAGAIN)
				continue;
			if(direction==READ)
				throw std::runtime_error("Error waiting for file descriptor to be ready to read");
			else
				throw std::runtime_error("Error waiting for file descriptor to be ready to write");
			*/
			if(direction==READ && FD_ISSET(fd,readset))
				return;
			if(direction==WRITE && FD_ISSET(fd,writeset))
				return;
			if(errno)
				std::cerr << devicePath << ": select gave error " << errno << std::endl;
		}
	}
	
public:
       DOMIOBuffer(const std::string& devPath, unsigned int minReadSize):
       fd(open(devPath.c_str(),O_RDWR|O_NONBLOCK)),
       minReadSize(minReadSize),
       readBuffer(new char_type[minReadSize]),
       readEOF(false),
       devicePath(devPath){
           if(fd<0) {
               perror(devPath.c_str());
               throw std::runtime_error("Failed to open DOR device "+devPath);
           }
           setg(readBuffer,readBuffer,readBuffer);
        }
	
	~DOMIOBuffer(){
		close(fd);
		delete[] readBuffer;
	}
	
	virtual std::streamsize xsputn(const char_type* s, std::streamsize n){
		std::streamsize amountWritten=0;
		while(n>0){
			waitReady(WRITE);
			int result=write(fd,s,n);
			if(result>0){
				n-=result;
				s+=result;
				amountWritten+=result;
				if(n<=0)
					return(amountWritten);
			}
			else{
				if(errno==EAGAIN || errno==EWOULDBLOCK)
					continue;
				//TODO: include error code, etc
				//throw std::runtime_error("Write Error");
				std::cerr << devicePath << ": write gave error " << errno << std::endl;
			}
		}
		return(amountWritten);
	}
	
    int underflow(){
        if(readEOF)
            return(std::char_traits<char>::eof());
        if (gptr() < egptr()) // buffer not exhausted
            return traits_type::to_int_type(*gptr());
        while(true){
            waitReady(READ);
            int result=read(fd,(void*)readBuffer,minReadSize);
            if(result<0){ //an error occurred
                if(errno==EAGAIN)
                    continue;
                std::cerr << devicePath << ": read gave result " << result;
                std::cerr << " and error " << errno << std::endl;
                throw std::runtime_error("Read Error");
            }
            else if (result==0)
                continue;
            else
                setg(readBuffer,readBuffer,readBuffer+result); // update the stream buffer
            break;
        }
        return traits_type::to_int_type(*gptr());
    }
};

#endif //include guard

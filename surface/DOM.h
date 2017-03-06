#ifndef DOM_H_INCLUDED
#define DOM_H_INCLUDED

#include <istream>
#include <ostream>
#include <fstream>
#include <sstream>

#include <zlib.h>

#include "DOMIOBuffer.h"

class DOM{
public:
	unsigned short cardNumber;
	unsigned short pairNumber;
	char domLabel;
	
	DOMIOBuffer dorBuffer;
	std::ostream send;
	std::istream recv;
	
	static std::string deviceFilePath(unsigned short cardNumber, unsigned short pairNumber, char domLabel){
		std::ostringstream ss;
		ss << "/dev/dhc" << cardNumber << 'w' << pairNumber << 'd' << domLabel;
		return(ss.str());
	}
	
	static std::string cardDirPath(unsigned short cardNumber){
		std::ostringstream ss;
		ss << "/proc/driver/domhub/card" << cardNumber;
		return(ss.str());
	}
	
	static std::string pairDirPath(unsigned short cardNumber, unsigned short pairNumber){
		std::ostringstream ss;
		ss << "/proc/driver/domhub/card" << cardNumber << "/pair" << pairNumber;
		return(ss.str());
	}
	
	static std::string domDirPath(unsigned short cardNumber, unsigned short pairNumber, char domLabel){
		std::ostringstream ss;
		ss << "/proc/driver/domhub/card" << cardNumber << "/pair" << pairNumber << "/dom" << domLabel;
		return(ss.str());
	}
	
	static unsigned int requiredMessageSize(){
		std::ifstream msgSizeFile("/proc/driver/domhub/bufsiz");
		if(!msgSizeFile.good())
			throw std::runtime_error("Failed to open /proc/driver/domhub/bufsiz");
		unsigned int size;
		msgSizeFile >> size;
		if(msgSizeFile.fail())
			throw std::runtime_error("Error parsing /proc/driver/domhub/bufsiz");
		return(size);
	}
	
	std::string cardDirPath() const{
		return(cardDirPath(cardNumber));
	}
	
	std::string pairDirPath() const{
		return(pairDirPath(cardNumber,pairNumber));
	}
	
	std::string domDirPath() const{
		return(domDirPath(cardNumber,pairNumber,domLabel));
	}
	
	DOM(const DOM&); //not copyable
	
public:
	DOM(unsigned short cardNumber, unsigned short pairNumber, char domLabel):
	cardNumber(cardNumber),
	pairNumber(pairNumber),
	domLabel(domLabel),
	dorBuffer(deviceFilePath(cardNumber,pairNumber,domLabel),requiredMessageSize()),
	send(&dorBuffer),
	recv(&dorBuffer)
	{
		if(!(send.good() && recv.good()))
			throw std::runtime_error("Failed to establish communications with DOR driver");
		recv.setf(std::ios_base::skipws);
		if(!communicating())
			throw std::runtime_error("DOM is not communicative");
	}
	
	struct endlManipulator{
		static const char* endlStr;
	};
	static endlManipulator endl;
	friend std::ostream& operator<<(std::ostream& os, const DOM::endlManipulator&);
	friend std::string operator+(const std::string s, const DOM::endlManipulator&);
	
	template <typename DataType>
	DOM& operator<<(const DataType& d){
		send << d;
		return(*this);
	}
	
	template <typename DataType>
	DOM& operator>>(DataType& d){
		recv >> d;
		return(*this);
	}
	
	bool getline(std::string& s){
		return(std::getline(recv,s).good());
	}
	
	bool recv_ok() const{
		return(recv.good());
	}
	
	bool send_ok() const{
		return(send.good());
	}
	
	bool checkDOMLocString(std::istream& is) const{
		std::string dummy;
		unsigned short card, pair;
		std::string label;
		is
		>> dummy //Card
		>> card 
		>> dummy //Pair
		>> pair 
		>> dummy //DOM
		>> label;
		return(!is.fail() 
		       && card==cardNumber 
		       && pair==pairNumber 
		       && !label.empty() && label[0]==domLabel);
	}
	
	std::string getMainboardID() const{
		const std::string path=domDirPath()+"/id";
		std::ifstream mbIDFile(path.c_str());
		if(!mbIDFile.good())
			throw std::runtime_error("Failed to open "+path);
		std::string dummy, id;
		if(!checkDOMLocString(mbIDFile))
			throw std::runtime_error("Error parsing "+path);
		mbIDFile 
		>> dummy //ID
		>> dummy //is
		>> id;
		if(mbIDFile.fail())
			throw std::runtime_error("Error parsing "+path);
		return(id);
	}
	
	bool communicating() const{
		const std::string path=domDirPath()+"/is-communicating";
		std::ifstream commFile(path.c_str());
		if(!commFile.good())
			throw std::runtime_error("Failed to open "+path);
		std::string dummy, result;
		if(!checkDOMLocString(commFile))
			throw std::runtime_error("Error parsing "+path);
		commFile 
		>> dummy //is
		>> result;
		if(commFile.fail())
			throw std::runtime_error("Error parsing "+path);
		return(result=="communicating");
	}
	
	//reads input until terminator or EOF is found
	std::string receive(const std::string& terminator, bool includeTerminator=true){
		bool wasSkippingWS=recv.flags()&std::ios_base::skipws;
		if(wasSkippingWS)
			recv.unsetf(std::ios_base::skipws);
		std::string data;
		//Search for the terminator the stupid way, one character at a time. 
		//This could be made a little better by inserting Boyer-Moore here, but we 
		//already do buffering inside DOMIOBuffer, and also the typical terminator 
		//strings are very short, so there doesn't seem to be much to gain
		char c;
		while(true){
			recv >> c;
			if(recv.fail() || recv.eof())
				break;
			data+=c;
			if(data.size()<terminator.size())
				continue;
			if(data.find(terminator,data.size()-terminator.size())!=std::string::npos)
				break;
		}
		if(wasSkippingWS)
			recv.setf(std::ios_base::skipws);
		if(!includeTerminator)
			return(data.substr(0,data.size()-terminator.size()));
		return(data);
	}
	
	std::string receive(const endlManipulator&, bool includeTerminator=true){
		return(receive(endlManipulator::endlStr,includeTerminator));
	}
	
	//reads zlib compressed input until compressed stream ends
	void zreceiveToStream(std::ostream& os){
		const std::streamsize readBlockSize=1024;
		//We expect the data to decompress, so we allocate the
		//output buffer to be larger than the input buffer.
		//According to Mark Adler (http://www.zlib.net/zlib_tech.html
		//retrieved August 29, 2014) zlib has a theoretical maximum
		//compression factor of 1032.
		const std::streamsize decompSize=1032*readBlockSize;
		char* readBuffer=new char[readBlockSize];
		char* decompBuffer=new char[decompSize];
		//for approximate exception safety
		//this should be replaced eventually by use of std::unique_ptr
		struct deleter{
			char* b;
			deleter(char* b):b(b){}
			~deleter(){ delete[] b; }
		} d1(readBuffer), d2(decompBuffer);
		
		z_stream zs;
		zs.next_in = Z_NULL;
		zs.avail_in = 0;
		zs.next_out = (unsigned char*)decompBuffer;
		zs.avail_out = decompSize;
        zs.zalloc = Z_NULL; 
        zs.zfree = Z_NULL; 
        zs.opaque = Z_NULL;
        
		bool firstBlock=true;
		int result=inflateInit(&zs);
		if(result!=Z_OK)
			throw std::runtime_error("Failed to initialize zlib decompression");
		struct inflateCleanup{ //ensure that inflateEnd is always called on zs
			z_stream* s;
			inflateCleanup(z_stream* s):s(s){}
			~inflateCleanup(){ inflateEnd(s); }
		} c(&zs);
		while(!recv.eof()){
			if(!zs.avail_in){
				timespec sleepTime={0,10000};
				nanosleep(&sleepTime,NULL);
				zs.next_in = (unsigned char*)readBuffer;
				
				//block until we can get at least one more byte. . .
				//(For the first read we need a substantial amount of data,
				//because zlib wants to see the entire stream header at one time)
				while(zs.avail_in<(firstBlock?readBlockSize:1)){
					recv.read(readBuffer,(firstBlock?readBlockSize:1)-zs.avail_in);
					zs.avail_in+=recv.gcount();
					if(recv.eof()) //eof!? This is a device driver!!!
						recv.clear();
				}
				firstBlock=false;
				
				//. . . and then opportunistically take as much more as we can
				if(readBlockSize-zs.avail_in)
					zs.avail_in+=recv.readsome(readBuffer+zs.avail_in,readBlockSize-zs.avail_in);
			}
			//hew closely to the zlib usage example
			do{
				zs.next_out = (unsigned char*)decompBuffer;
				zs.avail_out = decompSize;
				result=inflate(&zs,Z_NO_FLUSH);
				if(result<Z_OK){
					std::ostringstream ss;
					ss << "Zlib decompression error: " << result;
					if(zs.msg!=Z_NULL)
						ss << " (" << zs.msg << ')';
					throw std::runtime_error(ss.str());
                }
				std::streamsize availData = decompSize-zs.avail_out;
				os.write(decompBuffer,availData);
			} while(zs.avail_out==0);
			if(result==Z_STREAM_END)
				break; //done!
		}
	}
	
	std::string zreceive(){
		std::ostringstream stream;
		zreceiveToStream(stream);
		return(stream.str());
	}
	
	//Try to put the DOM in the iceboot state
	bool iceboot(){
		//try to determine state of DOM from command prompt string
		//Some redundancy here to deal with the case of configboot power-on info
		send << endl;
		receive("\n");
		send << endl << endl << endl;
		receive("\n");
		receive("\n");
		std::string prompt=receive("\n");
		
		if(prompt.empty())
			return(false);
		switch(prompt[0]){
			case '>':
				//already in iceboot
				//the following is supposed to "Sync I/O"
				send << "ls" << endl;
				receive("ls"+endl);
				break;
			case '#':
				//in configboot, need to move to iceboot
				send << "r" << endl;
				receive(">");
				break;
			default:
				return(false);
		}
		return(true);
	}
};

std::ostream& operator<<(std::ostream& os, const DOM::endlManipulator&);
std::string operator+(const std::string s, const DOM::endlManipulator&);
std::string operator+(const DOM::endlManipulator&, const std::string s);

#endif //include guard

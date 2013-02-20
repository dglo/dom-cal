#ifndef SIMPLEDRAWING_H_INCLUDED
#define SIMPLEDRAWING_H_INCLUDED

#include <algorithm>
#include <cassert>
#include <map>
#include <ostream>
#include <string>
#include <stdint.h>

struct color{
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
	
	color():
	r(255),g(255),b(255),a(255){}
	
	color(uint8_t r, uint8_t g, uint8_t b):
	r(r),g(g),b(b),a(255){}
	
	color(uint8_t r, uint8_t g, uint8_t b, uint8_t a):
	r(r),g(g),b(b),a(a){}
	
	static const color transparent;
	static const color black;
	static const color white;
	static const color red;
	static const color orange;
	static const color yellow;
	static const color green;
	static const color cyan;
	static const color blue;
	static const color purple;
};

bool operator==(const color& c1, const color& c2);
bool operator!=(const color& c1, const color& c2);
//std::ostream& operator<<(std::ostream& os, const color& c);

class bitmapFont{
public:
	struct glyph{
		static const unsigned int glyphWidth;
		static const unsigned int glyphHeight;
		static const unsigned int descenderHeight;
		static const unsigned int lineSpacing;
		
		uint64_t data;
		uint32_t descender;
		bool hasDescender;
		int8_t kern1;
		int8_t kern2;
		
		void decideDescender(){
			if(data&0x1)
				hasDescender=true;
			data&=0xFFFFFFFFFFFFFFFEull;
		}
		
		void decodePackedKerning(uint8_t k){
			kern1=(k&0x70)>>4;
			if(k&0x80)
				kern1=-kern1;
			kern2=(k&0x7);
			if(k&0x8)
				kern2=-kern2;
		}
		
		glyph():data(0),hasDescender(false),kern1(0),kern2(0){}
		
		glyph(uint64_t g, int8_t k1, int8_t k2):
		data(g&0xFFFFFFFFFFFFFFFEull),hasDescender(false),kern1(k1),kern2(k2){}
		
		glyph(uint64_t g, uint32_t d, int8_t k1, int8_t k2):
		data(g&0xFFFFFFFFFFFFFFFEull),hasDescender(true),descender(d),kern1(k1),kern2(k2){}
		
		friend std::istream& operator>>(std::istream& is, bitmapFont::glyph& g);
	};
private:
	std::map<char,glyph> data;
	
	static int kern(int k1, int k2){
		if((k1>=0) && (k2>=0))
			return(std::max(k1,k2));
		return(k1+k2);
	}
	
public:
	bitmapFont(){}
	
	template<typename CharIterator, typename GlyphIterator>
	bitmapFont(CharIterator cbegin, CharIterator cend, GlyphIterator gbegin, GlyphIterator gend){
		for(; cbegin!=cend && gbegin!=gend; cbegin++, gbegin++)
			data.insert(std::make_pair(*cbegin,*gbegin));
	}
	
	void loadFromFile(const std::string& path);
	int offsetNext(char c1, char c2) const;
	const glyph& getGlyph(char c) const;
	
};

std::istream& operator>>(std::istream& is, bitmapFont::glyph& g);

class bitmapImage{
private:
	unsigned int width;
	unsigned int height;
	color* data;
	
	unsigned int idx(unsigned int x, unsigned int y) const{
		assert(x<width);
		assert(y<height);
		return(x+width*(height-y-1));
	}
	
public:
	bitmapImage(unsigned int w, unsigned int h, const color& fillColor=color::white):
	width(w),height(h),data(new color[width*height]){
		if(fillColor!=color())
			std::fill_n(data,width*height,fillColor);
	}
	
	~bitmapImage(){
		delete[] data;
	}
	
	unsigned int getWidth() const{
		return(width);
	}
	
	unsigned int getHeight() const{
		return(height);
	}
	
	const color* getData() const{
		return(data);
	}
	
	const color& getPixel(unsigned int x, unsigned int y) const{
		if(x>=width || y>height)
			return(color::transparent);
		return(data[idx(x,y)]);
	}
	
	void setPixel(unsigned int x, unsigned int y, const color& c){
		if(x>=width || y>height)
			return;
		data[idx(x,y)]=c;
	}
	
	void drawLine(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, const color& c);
	
	void drawGlyph(unsigned int x, unsigned int y, const bitmapFont::glyph& g, const color& c);
	
	int computeStringWidth(const std::string& s, const bitmapFont& font);
	int computeStringHeight(const std::string& s, const bitmapFont& font);
	std::pair<int,int> computeStringDimensions(const std::string& s, const bitmapFont& font);
	void drawString(unsigned int x, unsigned int y, const std::string& s, const color& c, const bitmapFont& font);
	
	void writePNG(const std::string& path);
};

extern bitmapFont basicFont;

#endif //include guard

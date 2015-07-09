#include "SimpleDrawing.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>

#include <png.h>

struct endianness_t{
	bool isLittle;
	endianness_t(){
		uint16_t test=0xFF00;
		isLittle=((uint8_t*)(&test))[0]==0;
	}
} endianness;

uint32_t swapBytes(uint32_t n){
	return(((n&0xFF)<<24)|((n&0xFF00)<<8)|((n&0xFF0000)>>8)|((n&0xFF000000)>>24));
}

uint64_t swapBytes(uint64_t n){
	return(((n&0xFF)<<56)|((n&0xFF00)<<40)|((n&0xFF0000)<<24)|((n&0xFF000000)<<8)
		   |((n&0xFF00000000ull)>>8)|((n&0xFF0000000000ull)>>24)|((n&0xFF000000000000ull)>>40)|((n&0xFF00000000000000ull)>>56));
}

const color color::transparent(0,0,0,0);
const color color::black(0,0,0);
const color color::white;
const color color::red(255,0,0);
const color color::orange(255,127,0);
const color color::yellow(255,255,0);
const color color::green(0,255,0);
const color color::cyan(0,255,255);
const color color::blue(0,0,255);
const color color::purple(63,0,191);

bool operator==(const color& c1, const color& c2){
	return(c1.r==c2.r && c1.g==c2.g && c1.b==c2.b && c1.a==c2.a);
}

bool operator!=(const color& c1, const color& c2){
	return(c1.r!=c2.r || c1.g!=c2.g || c1.b!=c2.b || c1.a!=c2.a);
}

const unsigned int bitmapFont::glyph::glyphWidth=7;
const unsigned int bitmapFont::glyph::glyphHeight=9;
const unsigned int bitmapFont::glyph::descenderHeight=4;
const unsigned int bitmapFont::glyph::lineSpacing=5;

std::istream& operator>>(std::istream& is, bitmapFont::glyph& g){
	is.read((char*)&g.data,sizeof(uint64_t));
	if(endianness.isLittle)
		g.data=swapBytes(g.data);
	g.decideDescender();
	if(g.hasDescender){
		is.read((char*)&g.descender,sizeof(uint32_t));
		if(endianness.isLittle)
			g.descender=swapBytes(g.descender);
	}
	uint8_t kerning;
	is.read((char*)&kerning,sizeof(uint8_t));
	g.decodePackedKerning(kerning);
	return(is);
}


void bitmapFont::loadFromFile(const std::string& path){
	std::ifstream infile(path.c_str(),std::ios::in|std::ios::binary);
	while(!infile.eof()){
		char c;
		glyph g;
		infile.read(&c,1);
		if(!infile)
			break;
		infile >> g;
		data.insert(std::make_pair(c,g));
	}
}

int bitmapFont::offsetNext(char c1, char c2) const{
	if(c1==0)
		return(0);
	std::map<char,glyph>::const_iterator i1,i2;
	i1=data.find(c1);
	i2=data.find(c2);
	if(i2==data.end()){
		if(i1!=data.end())
			return(glyph::glyphWidth+i1->second.kern2);
		return(glyph::glyphWidth);
	}
	if(i1==data.end())
		return(glyph::glyphWidth+i2->second.kern1);
	return(glyph::glyphWidth+kern(i1->second.kern2,i2->second.kern1));
}

const bitmapFont::glyph& bitmapFont::getGlyph(char c) const{
	std::map<char,glyph>::const_iterator it;
	it=data.find(c);
	assert(it!=data.end()); //TODO: better error handling
	return(it->second);
}


//just Bresenham's Line Algorithm, as seen on Wikipedia
void bitmapImage::drawLine(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, const color& c){
	int dx=(x1>x2?x1-x2:x2-x1);
	int dy=(y1>y2?y1-y2:y2-y1);
	int sx=(x1>x2?-1:1);
	int sy=(y1>y2?-1:1);
	int err=dx-dy;
	
	while(x1!=x2 || y1!=y2){
		setPixel(x1,y1,c);
		int err2=2*err;
		if(err2>-dy){
			err-=dy;
			x1+=sx;
		}
		if(err2<dx){
			err+=dx;
			y1+=sy;
		}
	}
	setPixel(x2,y2,c);
}

void bitmapImage::drawGlyph(unsigned int x, unsigned int y, const bitmapFont::glyph& g, const color& c){
	unsigned int xp=x;
	unsigned int yp=y+bitmapFont::glyph::glyphHeight-1;
	int n=0;
	uint64_t mask=1ull<<63;
	for(int i=0; i<(bitmapFont::glyph::glyphHeight*bitmapFont::glyph::glyphWidth); i++){
		if(g.data&mask)
			setPixel(xp,yp,c);
		xp++;
		n++;
		if(n==bitmapFont::glyph::glyphWidth){
			//descend to the next row
			n=0;
			xp-=bitmapFont::glyph::glyphWidth;
			yp-=1;
		}
		mask>>=1;
	}
	if(g.hasDescender){
		xp=x;
		yp=y-1;
		n=0;
		uint32_t mask=1ul<<31;
		for(int i=0; i<(bitmapFont::glyph::descenderHeight*bitmapFont::glyph::glyphWidth); i++){
			if(g.descender&mask)
				setPixel(xp,yp,c);
			xp++;
			n++;
			if(n==bitmapFont::glyph::glyphWidth){
				//descend to the next row
				n=0;
				xp-=bitmapFont::glyph::glyphWidth;
				yp-=1;
			}
			mask>>=1;
		}
	}
}

int bitmapImage::computeStringHeight(const std::string& s, const bitmapFont& font){
	int height=bitmapFont::glyph::glyphHeight+bitmapFont::glyph::lineSpacing;
	for(int i=0; i<s.length(); i++){
		char cur=s[i];
		if(cur=='\n' || cur=='\v')
			height+=bitmapFont::glyph::glyphHeight+bitmapFont::glyph::lineSpacing;
	}
	return(height);
}

int bitmapImage::computeStringWidth(const std::string& s, const bitmapFont& font){
	return(computeStringDimensions(s,font).first);
}

#include <iostream>

std::pair<int,int> bitmapImage::computeStringDimensions(const std::string& s, const bitmapFont& font){
	std::pair<int,int> d(0,bitmapFont::glyph::glyphHeight+bitmapFont::glyph::lineSpacing);
	int lineWidth=0;
	char prev=0,cur;
	for(int i=0; i<s.length(); i++){
		cur=s[i];
		if(cur>=' '){
			lineWidth+=font.offsetNext(prev,cur);
			prev=cur;
		}
		else{
			if(cur=='\n' || cur=='\r'){
				if(prev!=0)
					lineWidth+=font.offsetNext(prev,cur);
				if(lineWidth>d.first)
					d.first=lineWidth;
				lineWidth=0;
				if(cur=='\n')
					d.second+=bitmapFont::glyph::glyphHeight+bitmapFont::glyph::lineSpacing;
			}
			else if(cur=='\t'){
				lineWidth+=font.offsetNext(prev,0);
				const unsigned int tabWidth=4*bitmapFont::glyph::glyphWidth;
				//locate the next tab stop
				int nextStop=(lineWidth/tabWidth+1)*tabWidth;
				//if the distance to the next stop is less than one full glyph width, 
				//go one stop further
				if((nextStop-lineWidth)<bitmapFont::glyph::glyphWidth)
					nextStop+=tabWidth;
				lineWidth=nextStop;
			}
			else if(cur=='\v'){
				lineWidth+=font.offsetNext(prev,0);
				d.second+=bitmapFont::glyph::glyphHeight+bitmapFont::glyph::lineSpacing;
			}
			else
				lineWidth+=bitmapFont::glyph::glyphWidth;
			prev=0;
		}
	}
	if(prev!=0 && cur>=' ')
		lineWidth+=font.offsetNext(prev,cur);
	if(lineWidth>d.first)
		d.first=lineWidth;
	return(d);
}

void bitmapImage::drawString(unsigned int x, unsigned int y, const std::string& s, const color& c, const bitmapFont& font){
	char prev=0,cur;
	unsigned int startX=x;
	for(int i=0; i<s.length(); i++){
		cur=s[i];
		if(cur>=' '){
			x+=font.offsetNext(prev,cur);
			drawGlyph(x,y,font.getGlyph(cur),c);
			prev=cur;
		}
		else{
			if(cur=='\n'){ //newline
				x=startX;
				y-=bitmapFont::glyph::glyphHeight+bitmapFont::glyph::lineSpacing;
			}
			else if(cur=='\r') //carriage return (yuck)
				x=startX;
			else if(cur=='\t'){ //tab
				x+=font.offsetNext(prev,0);
				const unsigned int tabWidth=4*bitmapFont::glyph::glyphWidth;
				//locate the next tab stop
				int nextStop=startX+((x-startX)/tabWidth+1)*tabWidth;
				//if the distance to the next stop is less than one full glyph width, 
				//go one stop further
				if((nextStop-x)<bitmapFont::glyph::glyphWidth)
					nextStop+=tabWidth;
				x=nextStop;
			}
			else if(cur=='\v'){ //VERTICAL TAB!!!
				x+=font.offsetNext(prev,0);
				y-=bitmapFont::glyph::glyphHeight+bitmapFont::glyph::lineSpacing;
			}
			else
				x+=bitmapFont::glyph::glyphWidth;
			prev=0;
		}
	}
}

void bitmapImage::writePNG(const std::string& path){
	FILE* fp=NULL;
	png_structp png_ptr=NULL;
	png_infop info_ptr=NULL;
	
	struct cleanup_t{
		FILE*& fp;
		png_structp& png_ptr;
		png_infop& info_ptr;
		
		cleanup_t(FILE*& fp, png_structp& png_ptr, png_infop& info_ptr):
		fp(fp),png_ptr(png_ptr),info_ptr(info_ptr){}
		
		~cleanup_t(){
			if (fp != NULL) fclose(fp);
			if (info_ptr != NULL) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
			if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		}
	} cleanup(fp,png_ptr,info_ptr);
	
	fp = fopen(path.c_str(), "wb");
	if (fp == NULL)
		throw std::runtime_error("Could not write to "+path);
	
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL)
		throw std::runtime_error("Could not allocate PNG write structure");
	
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
		throw std::runtime_error("Could not allocate PNG info structure");
	
	//Ugh, set up 'exception handling'
	if (setjmp(png_jmpbuf(png_ptr)))
		throw std::runtime_error("Could not write PNG data");
	
	png_init_io(png_ptr, fp);
	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB_ALPHA, 
				 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_write_info(png_ptr, info_ptr);
	
	png_bytep rowData=(png_bytep)data;
	for(unsigned int row=0; row<height; row++){
		png_write_row(png_ptr, rowData);
		rowData+=width*sizeof(color);
	}
	png_write_end(png_ptr, NULL);
}

const unsigned int nPrintableASCIIChars(95);
char printableASCII[nPrintableASCIIChars]={
	' ','!','"','#','$','%','&','\'','(',')',
	'*','+',',','-','.','/','0','1','2','3',
	'4','5','6','7','8','9',':',';','<','=',
	'>','?','@','A','B','C','D','E','F','G',
	'H','I','J','K','L','M','N','O','P','Q',
	'R','S','T','U','V','W','X','Y','Z','[',
	'\\',']','^','_','`','a','b','c','d','e',
	'f','g','h','i','j','k','l','m','n','o',
	'p','q','r','s','t','u','v','w','x','y',
	'z','{','|','}','~'};
bitmapFont::glyph basicGlyphs[nPrintableASCIIChars]={
	bitmapFont::glyph(0x0000000000000000ull,0,0), //' '
	bitmapFont::glyph(0x1020408102040010ull,-2,-2), //!
	bitmapFont::glyph(0x2850A00000000000ull,-1,-1), //"
	bitmapFont::glyph(0x142857F2853FA850ull,1,1), //#
	bitmapFont::glyph(0x1071120380911C10ull,0,0), //$
	bitmapFont::glyph(0x20A4904104124A08ull,1,1), //%
	bitmapFont::glyph(0x309101061222C672ull,1,1), //&
	bitmapFont::glyph(0x1020400000000000ull,-2,-2), //'
	bitmapFont::glyph(0x0820820408101011ull,0x08000000,0,-1), //(
	bitmapFont::glyph(0x2020202040810411ull,0x20000000,-1,0), //)
	bitmapFont::glyph(0x10A8E1C542000000ull,0,0), //*
	bitmapFont::glyph(0x00004087C2040000ull,0,0), //+
	bitmapFont::glyph(0x0000000000000C19ull,0x08200000,-1,0), //,
	bitmapFont::glyph(0x00000007C0000000ull,0,0), //-
	bitmapFont::glyph(0x0000000000000C18ull,-1,0), //.
	bitmapFont::glyph(0x0408204104082040ull,0,0), ///
	bitmapFont::glyph(0x3092142850A12430ull,1,0), //0
	bitmapFont::glyph(0x30A040810204087Cull,0,0), //1
	bitmapFont::glyph(0x780810208208207Cull,0,0), //2
	bitmapFont::glyph(0x7808104301010278ull,0,0), //3
	bitmapFont::glyph(0x0830A2489F820408ull,1,0), //4
	bitmapFont::glyph(0x7C81038080810470ull,0,0), //5
	bitmapFont::glyph(0x3C82058C90A12430ull,1,0), //6
	bitmapFont::glyph(0xFC08208104082040ull,1,0), //7
	bitmapFont::glyph(0x790A124790A14278ull,1,0), //8
	bitmapFont::glyph(0x30921424C68104F0ull,1,0), //9
	bitmapFont::glyph(0x000000C180000C18ull,-1,0), //:
	bitmapFont::glyph(0x000000C180000C19ull,0x08200000,-1,0), //;
	bitmapFont::glyph(0x0000318C06030000ull,1,0), //<
	bitmapFont::glyph(0x000007E01F800000ull,1,0), //=
	bitmapFont::glyph(0x00030180C6300000ull,1,0), //>
	bitmapFont::glyph(0x7908104104080020ull,1,0), //?
	bitmapFont::glyph(0x388A752A53A02238ull,1,0), //@
	bitmapFont::glyph(0x1020A1444FA0C182ull,1,1), //A
	bitmapFont::glyph(0xF90A142F90A142F8ull,1,0), //B
	bitmapFont::glyph(0x3C8604081020213Cull,1,1), //C
	bitmapFont::glyph(0xF112142850A144F0ull,1,0), //D
	bitmapFont::glyph(0xFD02040F102040FCull,1,0), //E
	bitmapFont::glyph(0xFD02040F10204080ull,1,0), //F
	bitmapFont::glyph(0x388A0C0811E0A238ull,1,1), //G
	bitmapFont::glyph(0x83060C1FF060C182ull,1,1), //H
	bitmapFont::glyph(0x3820408102040838ull,-1,-1), //I
	bitmapFont::glyph(0x3E08102850A12430ull,1,1), //J
	bitmapFont::glyph(0x8512450C14244484ull,1,0), //K
	bitmapFont::glyph(0x81020408102040FCull,1,0), //L
	bitmapFont::glyph(0x838F1D5AB564C982ull,1,1), //M
	bitmapFont::glyph(0x83868D193162C382ull,1,1), //N
	bitmapFont::glyph(0x388A0C183060A238ull,1,1), //O
	bitmapFont::glyph(0x7889122788102040ull,0,0), //P
	bitmapFont::glyph(0x388A0C183060A239ull,0x080C0000,1,1), //Q
	bitmapFont::glyph(0x788912278C142444ull,0,0), //R
	bitmapFont::glyph(0x790A040780814278ull,1,0), //S
	bitmapFont::glyph(0xFE20408102040810ull,1,1), //T
	bitmapFont::glyph(0x83060C183060A238ull,1,1), //U
	bitmapFont::glyph(0x83051222850A0810ull,1,1), //V
	bitmapFont::glyph(0x83060AA54A8A1428ull,1,1), //W
	bitmapFont::glyph(0x8305114105114182ull,1,1), //X
	bitmapFont::glyph(0x8305114102040810ull,1,1), //Y
	bitmapFont::glyph(0xFE041041041040FEull,1,1), //Z
	bitmapFont::glyph(0x3840810204081021ull,0x38000000,-1,-1), //[
	bitmapFont::glyph(0x4080810101020204ull,0,0), //\ extra non-whitespace to appease the preprocessor
	bitmapFont::glyph(0x3810204081020409ull,0x38000000,-1,-1), //]
	bitmapFont::glyph(0x1051100000000000ull,0,0), //^
	bitmapFont::glyph(0x0000000000000000ull,0xFE000000,0,0), //_
	bitmapFont::glyph(0x2020200000000000ull,-1,-1), //`
	bitmapFont::glyph(0x0000F22448912634ull,0,0), //a
	bitmapFont::glyph(0x4081E22448913258ull,0,0), //b
	bitmapFont::glyph(0x0000E22408102238ull,0,0), //c
	bitmapFont::glyph(0x0408F22448912634ull,0,0), //d
	bitmapFont::glyph(0x0000E2244F90203Cull,0,0), //e
	bitmapFont::glyph(0x184883C204081020ull,0,0), //f
	bitmapFont::glyph(0x0000F22448912635ull,0x0488E000,0,0), //g
	bitmapFont::glyph(0x4081632448912244ull,0,0), //h
	bitmapFont::glyph(0x0020008102040810ull,-2,-2), //i
	bitmapFont::glyph(0x0010004081020408ull,0x4890C000,-1,-1), //j
	bitmapFont::glyph(0x408112450C142444ull,0,0), //k
	bitmapFont::glyph(0x1020408102040818ull,-2,-2), //l
	bitmapFont::glyph(0x0002B6993264C992ull,1,1), //m
	bitmapFont::glyph(0x0001632448912244ull,0,0), //n
	bitmapFont::glyph(0x0000E22448912238ull,0,0), //o
	bitmapFont::glyph(0x0002E62850A162B9ull,0x81000000,1,0), //p
	bitmapFont::glyph(0x0000F22448912635ull,0x4080000,0,0), //q
	bitmapFont::glyph(0x0001B18204081070ull,0,0), //r
	bitmapFont::glyph(0x0000E20406020470ull,0,-1), //s
	bitmapFont::glyph(0x1021F0810204080Cull,0,0), //t
	bitmapFont::glyph(0x0001122448912634ull,0,0), //u
	bitmapFont::glyph(0x00011224450A0810ull,0,0), //v
	bitmapFont::glyph(0x00020C192A951428ull,1,1), //w
	bitmapFont::glyph(0x00011222820A2244ull,0,0), //x
	bitmapFont::glyph(0x00011224450A0811ull,0x20410000,0,0), //y
	bitmapFont::glyph(0x0001F0208208207Cull,0,0), //z
	bitmapFont::glyph(0x0C2040810C040811ull,0x010203000,0,0), //{
	bitmapFont::glyph(0x1020408102040811ull,0x10000000,-2,-2), //|
	bitmapFont::glyph(0x6020408101840810ull,0x10C00000,0,0), //}
	bitmapFont::glyph(0x0041504000000000ull,0,0) //~
};

bitmapFont basicFont(printableASCII,printableASCII+nPrintableASCIIChars,basicGlyphs,basicGlyphs+nPrintableASCIIChars);

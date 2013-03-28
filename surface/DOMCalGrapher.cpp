#include <cmath>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <memory>

#include <sys/stat.h>

#include "DOMListing.h"
#include "DOMCalParser.h"
#include "SimpleDrawing.h"

std::auto_ptr<bitmapImage> graphSPEHistogram(const hv_histogram& hist){
	std::auto_ptr<bitmapImage> img(new bitmapImage(300,300,color::white));
	const float verticalScale=5.0; //counts per pixel
	
	//draw histogram
	for(int i=0; i<hist.bin_count && i<250; i++){
		int count=(int)(hist.y_data[i]/verticalScale);
		if(count>249)
			count=249;
		else if(count<0)
			count=0;
		img->drawLine(50+i,50,50+i,50+count,color::blue);
	}
	
	//draw fit curve
	const color& fitColor=(hist.convergent?color::green:color::red);
	int fitPrevious=-1;
	for(int i = 0; i<250; i++) {
		bool draw=true;
		float x = hist.x_data[i];
		int fitY=(int)(((hist.fit[0]*exp(-1*hist.fit[1]*x))+(hist.fit[2]*exp(-1*pow(x - hist.fit[3], 2)*hist.fit[4])))/verticalScale);
		if(fitY<1)
			fitY=1;
		else if(fitY>247)
			fitY=247;
		img->drawLine(50+i,49+fitY,50+i,51+fitY,fitColor);
		if(i && fitPrevious!=-1)
			img->drawLine(50+i,50+fitPrevious,50+i,50+fitY,fitColor);
		fitPrevious = fitY;
	}
	
	//draw axes
	img->drawLine(50,50,299,50,color::black);
	img->drawLine(50,50,50,299,color::black);
	//draw tick marks and axis labels
	for(int i=0; i<5; i++){
		img->drawLine(50*(i+1),48,50*(i+1),52,color::black); //x tick
		img->drawLine(48,50*(i+1),52,50*(i+1),color::black); //y tick
		
		std::string label;
		int width;
		//x-axis label
		label=lexical_cast<std::string>(hist.x_data[50*i]);
		width=img->computeStringWidth(label,basicFont);
		img->drawString(50*(i+1)-width/2,25-bitmapFont::glyph::glyphHeight/2,label,color::black,basicFont);
		//y-axis label
		label=lexical_cast<std::string>(50*i*verticalScale);
		width=img->computeStringWidth(label,basicFont);
		img->drawString(25-width/2,50*(i+1)-bitmapFont::glyph::glyphHeight/2,label,color::black,basicFont);
	}
	
	//draw other useful information
	std::string summary="PV="+lexical_cast<std::string>(((int)(10*hist.pv+0.5))/10.0)+"\n"
	                    "Noise="+lexical_cast<std::string>((int)hist.noise_rate)+"Hz\n"
	                    "Mean="+lexical_cast<std::string>(((int)(10*hist.fit[3]+0.5))/10.0)+"pC\n"
	                    "HV="+lexical_cast<std::string>(hist.voltage)+"V";
	int summaryWidth=img->computeStringWidth(summary,basicFont);
	img->drawString(295-summaryWidth,295-bitmapFont::glyph::glyphHeight,summary,color::black,basicFont);
	
	return(img);
}

int logXScale(double x){
	const double lmin=log(800.0);
	const double lmax=log(1800.0);
	return(50+(int)(200*((log(x)-lmin)/(lmax-lmin))));
}

int logYScale(double y){
	const double lmin=log(1e6);
	const double lmax=log(1e9);
	return(50+(int)(220*((log(y)-lmin)/(lmax-lmin))));
}

double gainForVoltage(const linear_fit& fit, double voltage){
	return(pow(10.0,fit.slope*log10(voltage)+fit.y_intercept));
}

double voltageForGain(const linear_fit& fit, double gain){
	return(pow(10.0,(log10(gain)-fit.y_intercept)/fit.slope));
}

std::auto_ptr<bitmapImage> graphHVGainFit(const DOMCalRecord& cal){
	std::auto_ptr<bitmapImage> img(new bitmapImage(300,300,color::white));
	
	const double xmin=800.0;
	const double xmax=2196;
	const double ymin=1.e6;
	const double ymax=2.4857e9;
	const double electronCharge=1.602E-19;
	
	//draw the fit, if it succeeded
	if(cal.hv_gain_valid){
		double x1=xmin, y1=gainForVoltage(cal.hv_gain_calib,x1), x2=xmax, y2=gainForVoltage(cal.hv_gain_calib,x2);
		if(y1<ymin){
			y1=ymin;
			x1=voltageForGain(cal.hv_gain_calib,y1);
		}
		else if(y1>ymax){
			y1=ymax;
			x1=voltageForGain(cal.hv_gain_calib,y1);
		}
		if(y2<ymin){
			y2=ymin;
			x2=voltageForGain(cal.hv_gain_calib,y2);
		}
		else if(y2>ymax){
			y2=ymax;
			x2=voltageForGain(cal.hv_gain_calib,y2);
		}
		//std::cout << "Endpoints will be (" << x1 << ',' << y1 << ") and (" << x2 << ',' << y2 << ')' << std::endl;
		img->drawLine(logXScale(x1),logYScale(y1),logXScale(x2),logYScale(y2),color(0,208,0));
	}
	
	//draw the points for histograms which are convergent
	for(int i=0; i<cal.num_histos; i++){
		if(cal.histogram_data[i].convergent){
			int x=logXScale(cal.histogram_data[i].voltage);
			int y=logYScale(1e-12*cal.histogram_data[i].fit[3]/electronCharge);
			img->drawLine(x-3,y,x+3,y,color(208,0,0));
			img->drawLine(x,y-3,x,y+3,color(208,0,0));
		}
	}
	
	//draw axes
	img->drawLine(50,50,299,50,color::black);
	img->drawLine(50,50,50,299,color::black);
	//draw tick marks and axis labels
	const unsigned int nxTicks=5, nyTicks=4;
	int xTicks[nxTicks] = {800, 1100, 1400, 1700, 2000};
	int yTicks[nyTicks] = {(int)1e6, (int)1e7, (int)1e8, (int)1e9};
	const char* yLabels[nyTicks] = {"1e6", "1e7", "1e8", "1e9"};
	for(int i=0; i<nxTicks; i++){
		int x=logXScale(xTicks[i]);
		img->drawLine(x,48,x,52,color::black);
		std::string label;
		int width;
		label=lexical_cast<std::string>(xTicks[i]);
		width=img->computeStringWidth(label,basicFont);
		img->drawString(logXScale(xTicks[i])-width/2,25-bitmapFont::glyph::glyphHeight/2,label,color::black,basicFont);
	}
	for(int i=0; i<nyTicks; i++){
		int y=logYScale(yTicks[i]);
		img->drawLine(48,y,52,y,color::black);
		int width;
		width=img->computeStringWidth(yLabels[i],basicFont);
		img->drawString(25-width/2,logYScale(yTicks[i])-bitmapFont::glyph::glyphHeight/2,yLabels[i],color::black,basicFont);
	}
	
	return(img);
}

int main(int argc, char* argv[]){
	std::string listingFilePath="~/nicknames.txt";
	std::string directory;
	
	if(argc==2)
		directory=argv[1];
	else if(argc==4 && argv[1]==std::string("-L")){
		listingFilePath=argv[2];
		directory=argv[3];
	}
	else{
		std::cerr << "Usage: DOMCalGrapher [-L DOM listing file] DOMCal_directory" << std::endl;
		return(1);
	}
	
	domListing knownDOMs;
	std::ifstream listingFile(listingFilePath.c_str());
	if(!listingFile)
		std::cerr << "Unable to read DOM list file; DOM names will be omitted" << std::endl;
	else
		loadDOMListing(listingFile,knownDOMs);
	
	errorListing errors;
	DOMCalParser parser(errors);
	std::map<int64_t,DOMCalRecord> calData;
	std::string histoDirPath;
	for(directory_iterator it(directory), end; it!=end; ++it){
		if(is_directory(*it) && it->path().stem()=="histos")
			histoDirPath=it->path().string();
		else if(it->path().extension()=="xml"){
			uint64_t mbID=0;
			if(!extractMainboardID(it->path().stem(),mbID))
				continue;
			try{
				calData.insert(std::make_pair(mbID,parser.parseDOMCalFile(it->path().string(),mbID)));
			} catch(xmlParseError& err){
				std::cerr << "Bleh, uncaught xmlParseError: " << err.what() << std::endl;
			}
		}
	}
	
	if(histoDirPath.empty()){ //if we didn't already fnd the histogram directory we'll have to create it
		size_t slashIdx = directory.rfind('/');
		std::string suffix;
		if(slashIdx!=std::string::npos && slashIdx<directory.size()-1)
			suffix="."+directory.substr(slashIdx+1);
		else
			suffix=directory;
		histoDirPath=directory+"/histos."+suffix;
		int err=mkdir(histoDirPath.c_str(),0755);
		if(err){
			std::cerr << "Error attempting to create histogram directory " << histoDirPath << "; unable to proceed" << std::endl;
			return(1);
		}
	}
	std::ofstream htmlPage((histoDirPath+"/index.html").c_str());
	htmlPage << "<html>\n"
	"<table border=2 cellpadding=9 cellspacing=2 valign=top>\n"
	"<tr align=\"center\">\n"
	"<th>Name</th>\n"
	"<th>Mainboard ID</th>\n"
	"<th>Charge Histograms</th><th></th><th></th><th></th><th></th><th></th><th></th><th></th><th></th><th></th><th></th><th></th>\n"
	"<th>Gain vs HV</th>\n"
	"</tr>\n";
	for(std::map<int64_t,DOMCalRecord>::const_iterator domIt=calData.begin(), domEnd=calData.end(); domIt!=domEnd; domIt++){
		std::stringstream ss;
		ss << std::hex << std::setfill('0') << std::setw(12) << domIt->first;
		std::string mbIDStr=ss.str();
		
		htmlPage << "<tr align=\"center\"><td>";
		domListing::const_iterator lit=knownDOMs.find(domIt->first);
		if(lit!=knownDOMs.end())
			htmlPage << lit->second.omkey << ' ' << lit->second.name;
		else
			htmlPage << "???";
		htmlPage << "</td>\n<td>" << mbIDStr << "</td>\n";
		
		bool linkedGainPlot=false;
		std::map<int,int> voltageCounts;
		std::auto_ptr<bitmapImage> hvGain=graphHVGainFit(domIt->second);
		hvGain->writePNG(histoDirPath+"/"+mbIDStr+".xml_hv.png");
		for(int i=0; i<domIt->second.num_histos; i++){
			int voltage=domIt->second.histogram_data[i].voltage;
			if(voltageCounts.find(voltage)!=voltageCounts.end() && voltageCounts.find(voltage)==voltageCounts.begin()){
				//if there's already an entry for this voltage, and it's the first (smallest) voltage, we need to start a new row in the HTML table
				if(!linkedGainPlot){
					htmlPage << "<td><a href=\"./" << mbIDStr << ".xml_hv.png\"><img src=./" << mbIDStr << ".xml_hv.png></a></td>\n";
					linkedGainPlot=true;
				}
				else
					htmlPage << "<td></td>\n";
				htmlPage << "</tr>\n<tr align=\"center\"><td></td>\n<td></td>\n";
			}
			std::auto_ptr<bitmapImage> histImg=graphSPEHistogram(domIt->second.histogram_data[i]);
			std::string histoImgName=mbIDStr+".xml"+lexical_cast<std::string>(voltage)+"."+lexical_cast<std::string>(voltageCounts[voltage])+".png";
			histImg->writePNG(histoDirPath+"/"+histoImgName);
			htmlPage << "<td><a href=\"./" << histoImgName << "\"><img src=./" << histoImgName << "></a></td>\n";
			voltageCounts[voltage]++;
		}
		if(!linkedGainPlot)
			htmlPage << "<td><a href=\"./" << mbIDStr << ".xml_hv.png\"><img src=./" << mbIDStr << ".xml_hv.png></a></td>\n";
		else
			htmlPage << "<td></td>\n";
		htmlPage << "</tr>\n";
	}
	htmlPage << "</table>\n</html>\n";
	htmlPage.close();
	
	return(0);
}

#include "DOMCalParser.h"

#include <iostream>
#include <iomanip>
#include <sstream>

#include <sys/stat.h>

std::ostream& operator<<(std::ostream& os, const mainboardID& m){
	std::ios_base::fmtflags store=os.flags();
	os.flags((std::ios::right | std::ios::hex) & ~std::ios::showbase);
	os << std::setfill('0') << std::setw(12) << m.id;
	os.flags(store); //restore all previous flags
	return(os);
}

bool operator==(const directory_iterator& d1, const directory_iterator& d2){
	if(d1.dirp==NULL && d2.dirp==NULL)
		return(true);
	if(!!d1.curItem.item != !!d2.curItem.item)
		return(false);
	if(d1.curItem.item==NULL && d2.curItem.item==NULL)
		return(true);
	if(d1.curItem.item->d_ino!=d2.curItem.item->d_ino)
		return(false);
	char* n1=d1.curItem.item->d_name,* n2=d2.curItem.item->d_name;
	for(int i=0; *n1 && *n2; i++)
		if(*n1++!=*n2++)
			return(false);
	return(true);
}

bool operator!=(const directory_iterator& d1, const directory_iterator& d2){
	return(!operator==(d1,d2));
}

bool is_directory(const directory_iterator::directory_entry& entry){
	if(!entry.item)
		return(false);
	return(entry.item->d_type==DT_DIR);
}

const char* severityString(severity_t s){
	switch(s){
		case WARNING: return "Warning";
		case ERROR: return "Error";
		case FATAL_ERROR: return "Fatal Error";
	}
}

std::ostream& operator<<(std::ostream& os, const error& e){
	os << severityString(e.severity);
	if(e.lineNumber)
		os << " (line " << e.lineNumber << "): ";
	else
		os << ": ";
	os << e.message;
	return(os);
}

value_error readValueWithError(xmlNode* xvalue){
	if(!xvalue)
		throw xmlParseError("Unable to extract a value with error from NULL XML element");
	value_error value;
	value.value=getNodeContents<float>(xvalue);
	value.error=getAttribute<float>(xvalue,"error");
	return(value);
}

linear_fit readLinearFit(xmlNode* xfit){
	if(!xfit)
		throw xmlParseError("Unable to extract linear fit from NULL XML element");
	if(getAttribute<std::string>(xfit,"model")!="linear")
		throw xmlParseError("Unable to extract xmlized fit with model '"+getAttribute<std::string>(xfit,"model")+"' as a linear fit");
	
	linear_fit fit;
	for(xmlNode* node=xfit->children; node!=NULL; node=node->next){
		std::string nodeName((char*)node->name);
		if(nodeName=="param"){
			std::string paramName=getAttribute<std::string>(node,"name");
			if(paramName=="slope")
				fit.slope=getNodeContents<float>(node);
			else if(paramName=="intercept")
				fit.y_intercept=getNodeContents<float>(node);
			else
				throw xmlParseError("Unknown parameter type '"+paramName+"' in linear fit");
		}
		else if(nodeName=="regression-coeff"){
			fit.r_squared=getNodeContents<float>(node);
		}
	}
	return(fit);
}

quadratic_fit readQuadraticFit(xmlNode* xfit){
	if(!xfit)
		throw xmlParseError("Unable to extract quadratic fit from NULL XML element");
	if(getAttribute<std::string>(xfit,"model")!="quadratic")
		throw xmlParseError("Unable to extract xmlized fit with model '"+getAttribute<std::string>(xfit,"model")+"' as a quadratic fit");
	
	quadratic_fit fit;
	for(xmlNode* node=xfit->children; node!=NULL; node=node->next){
		std::string nodeName((char*)node->name);
		if(nodeName=="param"){
			std::string paramName=getAttribute<std::string>(node,"name");
			if(paramName=="c0")
				fit.c0=getNodeContents<float>(node);
			else if(paramName=="c1")
				fit.c1=getNodeContents<float>(node);
			else if(paramName=="c2")
				fit.c2=getNodeContents<float>(node);
			else
				throw xmlParseError("Unknown parameter type '"+paramName+"' in quadratic fit");
		}
		else if(nodeName=="regression-coeff"){
			fit.r_squared=getNodeContents<float>(node);
		}
	}
	return(fit);
}

void DOMCalParser::parseATWDBinGain(DOMCalRecord& data, xmlNode* node){
	try{
		unsigned int chip_id,channel,bin;
		chip_id = getAttributeWithErrorContext<unsigned int>(node,"id",error(ERROR,"Invalid ATWD bin gain chip id: "));
		channel = getAttributeWithErrorContext<unsigned int>(node,"channel",error(ERROR,"Invalid ATWD bin gain channel number : "));
		bin = getAttributeWithErrorContext<unsigned int>(node,"bin",error(ERROR,"Invalid ATWD bin gain bin number: "));
		linear_fit fit = callWithErrorContext(readLinearFit,firstChild(node,"fit"),error(ERROR,"Bad ATWD bin gain fit: "));
		switch(chip_id){
			case 0:
				data.atwd0_gain_calib[channel][bin] = fit;
				data.read.ATWD0GainCalib[channel][bin] = true;
				break;
			case 1:
				data.atwd1_gain_calib[channel][bin] = fit;
				data.read.ATWD1GainCalib[channel][bin] = true;
				break;
			default:
				addError(error(WARNING,"Out of range ATWD chip id '"+getAttribute<std::string>(node,"id")+"'"));
		}
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseDACSetting(DOMCalRecord& data, xmlNode* node){
	try{
		unsigned int channel = getAttributeWithErrorContext<unsigned int>(node,"channel",error(ERROR,"Invalid DAC channel number: "));
		if(channel>=16){
			addError(error(ERROR,"Out of range DAC channel number '"+lexical_cast<std::string>(channel)+"'"));
			return;
		}
		data.dac_values[channel] = callWithErrorContext(getNodeContents<unsigned int>,node,error(ERROR,"Invalid DAC value: "));
		data.read.DACValue[channel] = true;
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseADCSetting(DOMCalRecord& data, xmlNode* node){
	try{
		unsigned int channel = getAttributeWithErrorContext<unsigned int>(node,"channel",error(ERROR,"Invalid ADC channel number: "));
		if(channel>=24){
			addError(error(ERROR,"Out of range ADC channel number '"+lexical_cast<std::string>(channel)+"'"));
			return;
		}
		data.adc_values[channel] = callWithErrorContext(getNodeContents<unsigned int>,node,error(ERROR,"Invalid ADC value: "));
		data.read.ADCValue[channel] = true;
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseChannelBaseline(xmlNode* b, unsigned int& chip_id, unsigned int& channel, float& baseline){
	chip_id = getAttributeWithErrorContext<unsigned int>(b,"atwd",error(ERROR,"Invalid ATWD baseline chip id: "));
	if(chip_id>=2){
		addError(error(ERROR,xmlGetLineNo(b),"Out of range ATWD baseline chip id: "+lexical_cast<std::string>(chip_id)));
		throw xmlParseError("");
	}
	channel = getAttributeWithErrorContext<unsigned int>(b,"channel",error(ERROR,"Invalid ATWD baseline channel number : "));
	if(channel>=3){
		addError(error(ERROR,xmlGetLineNo(b),"Out of range ATWD baseline channel number: "+lexical_cast<std::string>(channel)));
		throw xmlParseError("");
	}
	baseline = getAttributeWithErrorContext<float>(b,"value",error(ERROR,"Invalid ATWD baseline value: "));
}

void DOMCalParser::parseBaseline(DOMCalRecord& data, xmlNode* node){
	try{
		float voltage = getAttributeWithErrorContext<unsigned int>(node,"voltage",error(ERROR,"Invalid ATWD baseline voltage: "));
		hv_baselines* baselineRec=NULL;
		if(voltage!=0.0){
			//allocate space for one more baseline record, and arrange to have baselineRec point to the new record
			baselineRec = new hv_baselines[data.num_baselines+1];
			if(data.baseline_data!=NULL)
				memcpy(baselineRec,data.baseline_data,data.num_baselines*sizeof(hv_baselines));
			delete[] data.baseline_data;
			data.baseline_data = baselineRec;
			baselineRec=data.baseline_data+data.num_baselines;
			data.num_baselines++;
		}
		for(xmlNode* b=firstChild(node,"base"); b; b=nextSibling(b,"base")){
			try{
				unsigned int chip_id, channel;
				float baseline;
				parseChannelBaseline(b, chip_id, channel, baseline);
				if(voltage==0.0){
					switch(chip_id){
						case 0:
							data.atwd0_baseline[channel]=baseline;
							data.read.ATWD0Baseline[channel] = true;
							break;
						case 1:
							data.atwd1_baseline[channel]=baseline;
							data.read.ATWD1Baseline[channel] = true;
							break;
					}
				}
				else{
					switch(chip_id){
						case 0:
							baselineRec->atwd0_hv_baseline[channel]=baseline;
							break;
						case 1:
							baselineRec->atwd1_hv_baseline[channel]=baseline;
							break;
					}
					data.read.HVBaselines=true;
				}
			} catch(xmlParseError& err){
				//stop the error here, assuming it has already been recorded
			}	
		}
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}	
}

void DOMCalParser::parseHVHistogram(DOMCalRecord& data, xmlNode* node){
	try{
		short voltage = getAttributeWithErrorContext<short>(node,"voltage",error(ERROR,"Invalid charge histogram voltage: "));
		unsigned int convergent = getAttributeWithErrorContext<unsigned int>(node,"convergent",error(ERROR,"Invalid charge histogram convergence flag: "));
		float peakToValley = getAttributeWithErrorContext<float>(node,"pv",error(ERROR,"Invalid charge histogram peak-to-valley ratio: "));
		unsigned int noiseRate = getAttributeWithErrorContext<unsigned int>(node,"noiseRate",error(ERROR,"Invalid charge histogram noise rate: "));
		unsigned int filled = getAttributeWithErrorContext<unsigned int>(node,"isFilled",error(ERROR,"Invalid charge histogram filled flag: "));
		
		float expAmp, expWidth, gaussAmp, gaussWidth, gaussMean;
		for(xmlNode* p=firstChild(node,"param"); p; p=nextSibling(p,"param")){
			try{
				std::string paramName = getAttributeWithErrorContext<std::string>(p,"name",error(ERROR,"Bad histogram fit parameter name: "));
				if(paramName=="exponential amplitude")
					expAmp = callWithErrorContext(getNodeContents<float>,p,error(ERROR,"Invalid hv fit exponential amplitude: "));
				else if(paramName=="exponential width")
					expWidth = callWithErrorContext(getNodeContents<float>,p,error(ERROR,"Invalid hv fit exponential width: "));
				else if(paramName=="gaussian amplitude")
					gaussAmp = callWithErrorContext(getNodeContents<float>,p,error(ERROR,"Invalid hv fit gaussian amplitude: "));
				else if(paramName=="gaussian width")
					gaussWidth = callWithErrorContext(getNodeContents<float>,p,error(ERROR,"Invalid hv fit gaussian width: "));
				else if(paramName=="gaussian mean")
					gaussMean = callWithErrorContext(getNodeContents<float>,p,error(ERROR,"Invalid hv fit gaussian mean: "));
			} catch(xmlParseError& err){
				//stop the error here, assuming it has already been recorded
			}
		}
		
		hv_histogram* histo=NULL;
		//allocate space for one more baseline record, and arrange to have baselineRec point to the new record
		histo = new hv_histogram[data.num_histos+1];
		if(data.histogram_data!=NULL)
			memcpy(histo,data.histogram_data,data.num_histos*sizeof(hv_histogram));
		delete[] data.histogram_data;
		data.histogram_data = histo;
		histo=data.histogram_data+data.num_histos;
		data.num_histos++;
		
		//read the actual contents of the histogram
		xmlNode* h=firstChild(node,"histogram");
		if(h==NULL){ //but not if it doesn't exist
			addError(error(WARNING,xmlGetLineNo(node),"No histogram samples found for "+lexical_cast<std::string>(voltage)+" V"));
			histo->bin_count=0;
			histo->x_data=NULL;
			histo->y_data=NULL;
		}
		else{
			histo->bin_count=getAttributeWithErrorContext<unsigned int>(h,"bins",error(ERROR,"Invalid number of histogram bins: "));
			histo->x_data=new float[histo->bin_count];
			histo->y_data=new float[histo->bin_count];
			int count=0;
			xmlNode* b=NULL;
			for(b=firstChild(h,"bin"); b && count<histo->bin_count; b=nextSibling(b,"bin"), count++){
				unsigned int idx=getAttributeWithErrorContext<unsigned int>(b,"num",error(ERROR,"Invalid histogram bin number: "));
				if(idx>=histo->bin_count){
					addError(error(ERROR,xmlGetLineNo(b),"Invalid histogram bin index ("+lexical_cast<std::string>(idx)+", maximum expected is "+lexical_cast<std::string>(histo->bin_count-1)+")"));
					continue;
				}
				histo->x_data[idx]=getAttributeWithErrorContext<float>(b,"charge",error(ERROR,"Invalid histogram bin charge: "));
				histo->y_data[idx]=getAttributeWithErrorContext<float>(b,"count",error(ERROR,"Invalid histogram bin count: "));
			}
			if(count<histo->bin_count){ //found too few bins
				addError(error(ERROR,xmlGetLineNo(h),"Found "+lexical_cast<std::string>(count)+" histogram bins when "+lexical_cast<std::string>(histo->bin_count)+" were expected"));
				histo->bin_count=count;
			}
			else if(b) //found too many bins
				addError(error(ERROR,xmlGetLineNo(h),"Found more histogram bins than expected ("+lexical_cast<std::string>(histo->bin_count)+" were expected)"));
		}
		
		histo->voltage = voltage;
		histo->convergent = convergent;
		histo->pv = peakToValley;
		histo->noise_rate = noiseRate;
		histo->is_filled = filled;
		histo->fit = new float[5];
		histo->fit[0] = expAmp;
		histo->fit[1] = expWidth;
		histo->fit[2] = gaussAmp;
		histo->fit[3] = gaussMean;
		histo->fit[4] = gaussWidth;
		
		data.read.HVHistograms=true;
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseAmplifier(DOMCalRecord& data, xmlNode* node){
	try{
		unsigned int channel = getAttributeWithErrorContext<unsigned int>(node,"channel",error(ERROR,"Invalid ATWD amplifier channel number: "));
		if(channel>=3){
			addError(error(ERROR,xmlGetLineNo(node),"Out of range ATWD amplifier channel number: "+lexical_cast<std::string>(channel)));
			return;
		}
		data.amplifier_calib[channel] = callWithErrorContext(readValueWithError,firstChild(node,"gain"),error(ERROR,"Bad ATWD amplifier gain: "));
		data.read.AmplifierCalib[channel] = true;
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseATWDDeltaT(DOMCalRecord& data, xmlNode* node){
	try{
		unsigned int chip_id = getAttributeWithErrorContext<unsigned int>(node,"id",error(ERROR,"Invalid ATWD amplifier channel number: "));
		if(chip_id>=2){
			addError(error(ERROR,xmlGetLineNo(node),"Out of range ATWD delta t chip id: "+lexical_cast<std::string>(chip_id)));
			return;
		}
		data.atwd_delta_t[chip_id] = callWithErrorContext(readValueWithError,firstChild(node,"delta_t"),error(ERROR,"Bad ATWD delta t: "));
		data.read.ATWDDeltaT[chip_id] = true;
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseATWDFrequency(DOMCalRecord& data, xmlNode* node){
	try{
		unsigned int chip_id = getAttributeWithErrorContext<unsigned int>(node,"atwd",error(ERROR,"Invalid ATWD amplifier channel number: "));
		if(chip_id>=2){
			addError(error(ERROR,xmlGetLineNo(node),"Out of range ATWD delta t chip id: "+lexical_cast<std::string>(chip_id)));
			return;
		}
		switch(chip_id){
			case 0:
				data.atwd0_freq_calib = callWithErrorContext(readQuadraticFit,firstChild(node,"fit"),error(ERROR,"Bad ATWD frequency calibration: "));
				data.read.ATWD0FreqCalib = true;
				break;
			case 1:
				data.atwd1_freq_calib = callWithErrorContext(readQuadraticFit,firstChild(node,"fit"),error(ERROR,"Bad ATWD frequency calibration: "));
				data.read.ATWD1FreqCalib = true;
				break;
		}
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseDiscriminator(DOMCalRecord& data, xmlNode* node){
	try{
		std::string disc=getAttributeWithErrorContext<std::string>(node,"id",error(ERROR,"Invalid discriminator id: "));
		if(disc=="spe"){
			data.spe_disc_calib = callWithErrorContext(readLinearFit,firstChild(node,"fit"),error(ERROR,"Bad SPE discriminator calibration: "));
			data.read.SPEDiscCalib = true;
		}
		else if(disc=="mpe"){
			data.mpe_disc_calib = callWithErrorContext(readLinearFit,firstChild(node,"fit"),error(ERROR,"Bad MPE discriminator calibration: "));
			data.read.MPEDiscCalib = true;
		}
		else{
			addError(error(ERROR,xmlGetLineNo(node),"Unrecognized discriminator id: "+disc));
		}
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseTemperature(DOMCalRecord& data, xmlNode* node){
	try{
		std::string format=getAttributeWithErrorContext<std::string>(node,"format",error(ERROR,"Invalid temperature format: "));
		if(format!="Kelvin")
			addError(error(ERROR,xmlGetLineNo(node),"Unrecognized temperature format: "+format));
		data.temp = callWithErrorContext(getNodeContents<float>,node,error(ERROR,"Bad temperature: "));
		data.read.temp = true;
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseTransitTime(DOMCalRecord& data, xmlNode* node){
	try{
		data.transit_calib_points = getAttributeWithErrorContext<unsigned int>(node,"num_pts",error(ERROR,"Bad number of transit time points: "));
		data.transit_calib = callWithErrorContext(readLinearFit,firstChild(node,"fit"),error(ERROR,"Bad transit time calibration: "));
		data.read.TransitTimeCalib = true;
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseHVGainCal(DOMCalRecord& data, xmlNode* node){
	try{
		data.hv_gain_calib = callWithErrorContext(readLinearFit,firstChild(node,"fit"),error(ERROR,"Bad HV-Gain calibration: "));
		data.read.HVGainCalib = true;
		data.hv_gain_valid = true;
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseImpedance(DOMCalRecord& data, xmlNode* node){
	try{
		std::string format=getAttributeWithErrorContext<std::string>(node,"format",error(ERROR,"Invalid impedance format: "));
		if(format!="Ohms")
			addError(error(ERROR,xmlGetLineNo(node),"Unrecognized impedance format: "+format));
		data.temp = callWithErrorContext(getNodeContents<float>,node,error(ERROR,"Bad impedance: "));
		data.read.impedance = true;
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseFADCGain(DOMCalRecord& data, xmlNode* node){
	try{
		data.fadc_gain = callWithErrorContext(readValueWithError,firstChild(node,"gain"),error(ERROR,"Bad FADC gain: "));
		data.read.FADCGain = true;
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseFADCBaseline(DOMCalRecord& data, xmlNode* node){
	try{
		data.fadc_baseline = callWithErrorContext(readLinearFit,firstChild(node,"fit"),error(ERROR,"Bad FADC baseline: "));
		data.read.FADCBaseline = true;
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseFADCDeltaT(DOMCalRecord& data, xmlNode* node){
	try{
		data.fadc_delta_t = callWithErrorContext(readValueWithError,firstChild(node,"delta_t"),error(ERROR,"Bad FADC gain: "));
		data.read.FADCDeltaT = true;
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseDAQBaselineBin(xmlNode* node, unsigned int& chip_id, unsigned int& channel, unsigned int& bin, float& sample){
	chip_id = getAttributeWithErrorContext<unsigned int>(node,"atwd",error(ERROR,"Invalid DAQ baseline chip id: "));
	if(chip_id>=2){
		addError(error(ERROR,xmlGetLineNo(node),"Out of range DAQ baseline chip id: "+lexical_cast<std::string>(chip_id)));
		throw xmlParseError("");
	}
	channel = getAttributeWithErrorContext<unsigned int>(node,"channel",error(ERROR,"Invalid DAQ baseline channel number : "));
	if(channel>=3){
		addError(error(ERROR,xmlGetLineNo(node),"Out of range DAQ baseline channel number: "+lexical_cast<std::string>(channel)));
		throw xmlParseError("");
	}
	bin = getAttributeWithErrorContext<unsigned int>(node,"bin",error(ERROR,"Invalid DAQ baseline bin number : "));
	if(bin>=128){
		addError(error(ERROR,xmlGetLineNo(node),"Out of range DAQ baseline bin number: "+lexical_cast<std::string>(bin)));
		throw xmlParseError("");
	}
	sample = callWithErrorContext(getNodeContents<float>,node,error(ERROR,"Bad DAQ baseline sample: "));
}

void DOMCalParser::parseDAQBaseline(DOMCalRecord& data, xmlNode* node){
	try{
		for(xmlNode* s=firstChild(node,"waveform"); s; s=nextSibling(s,"waveform")){
			try{
				unsigned int chip_id, channel, bin;
				float sample;
				parseDAQBaselineBin(s, chip_id, channel, bin, sample);
				switch(chip_id){
					case 0:
						data.atwd0_daq_baseline_wf[channel][bin]=sample;
						data.read.ATWD0DAQBaseline[channel][bin] = true;
						break;
					case 1:
						data.atwd1_daq_baseline_wf[channel][bin]=sample;
						data.read.ATWD1DAQBaseline[channel][bin] = true;
						break;
				}
			} catch(xmlParseError& err){
				//stop the error here, assuming it has already been recorded
			}	
		}
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}	
}

void DOMCalParser::parsePMTDiscCal(DOMCalRecord& data, xmlNode* node){
	try{
		data.pmt_disc_calib_num_pts = getAttributeWithErrorContext<unsigned int>(node,"num_pts",error(ERROR,"Bad number of PMT discriminator points: "));
		data.pmt_disc_calib = callWithErrorContext(readLinearFit,firstChild(node,"fit"),error(ERROR,"Bad PMT discriminator calibration: "));
		data.read.PMTDiscCalib = true;
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseMainboardID(DOMCalRecord& data, xmlNode* node){
	try{
		std::string strID = callWithErrorContext(getNodeContents<std::string>,node,error(ERROR,"Bad mainboard ID: "));
		if(strID.length()>12){
			addError(error(ERROR,xmlGetLineNo(node),"Mainboard ID string too long: "+strID));
			return;
		}
		strncpy(data.dom_id,strID.c_str(),13);
		data.read.domID = true;
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseDate(DOMCalRecord& data, xmlNode* node){
	try{
		std::string strDate = callWithErrorContext(getNodeContents<std::string>,node,error(ERROR,"Bad date: "));
		std::istringstream dateStream(strDate);
		char dummy;
		dateStream >> data.day >> dummy >> data.month >> dummy >> data.year;
		if(dateStream.fail()){
			addError(error(ERROR,xmlGetLineNo(node),"Malformed date: "+strDate));
			return;
		}
		data.read.date = true;
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseTime(DOMCalRecord& data, xmlNode* node){
	try{
		std::string strDate = callWithErrorContext(getNodeContents<std::string>,node,error(ERROR,"Bad date: "));
		std::istringstream dateStream(strDate);
		char dummy;
		dateStream >> data.hour >> dummy >> data.minute >> dummy >> data.second;
		if(dateStream.fail()){
			addError(error(ERROR,xmlGetLineNo(node),"Malformed time: "+strDate));
			return;
		}
		data.read.time = true;
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void xmlErrorCallback(void* ctx, xmlError* err);

DOMCalRecord DOMCalParser::parseDOMCalFile(const std::string& file, uint64_t mbID){
	currentMbID = mbID;
	fatalErrorOccurred=false;
	//std::cout << "Reading " << file << std::endl;
	xmlLineNumbersDefault(1); //we want line numbers
	xmlSetStructuredErrorFunc(this,&xmlErrorCallback);
	raiiHandle<xmlDoc> tree(xmlReadFile(file.c_str(),NULL,XML_PARSE_RECOVER|XML_PARSE_PEDANTIC),&xmlFreeDoc);
	
	DOMCalRecord data;
	if(!tree || fatalErrorOccurred){
		errors.addError(mbID,error(FATAL_ERROR,"Failed to parse '"+file+"'"));
		return(data);
	}
	xmlNode* root = xmlDocGetRootElement(tree.get());
	
	try{
		//Ignore DOMCal version
		//data.SetDOMCalVersion(getAttribute<std::string>(root,"version"));
		data.read.exists=true;
		
		for(xmlNode* node=root->children; node!=NULL; node=node->next){
			if(node->type!=XML_ELEMENT_NODE)
				continue;
			std::string nodeName((char*)node->name);
			
			if(nodeName=="atwd") //768
				parseATWDBinGain(data,node);
			else if(nodeName=="adc") //24
				parseADCSetting(data,node);
			else if(nodeName=="dac") //16
				parseDACSetting(data,node);
			else if(nodeName=="baseline") //~10
				parseBaseline(data,node);
			else if(nodeName=="histo") //~10
				parseHVHistogram(data,node);
			else if(nodeName=="amplifier") //3
				parseAmplifier(data,node);
			else if(nodeName=="atwd_delta_t") //2
				parseATWDDeltaT(data,node);
			else if(nodeName=="atwdfreq") //2
				parseATWDFrequency(data,node);
			else if(nodeName=="discriminator") //2
				parseDiscriminator(data,node);
			else if(nodeName=="temperature") //1
				parseTemperature(data,node);
			else if(nodeName=="pmtTransitTime") //1
				parseTransitTime(data,node);
			else if(nodeName=="hvGainCal") //1
				parseHVGainCal(data,node);
			else if(nodeName=="frontEndImpedance") //1
				parseImpedance(data,node);
			else if(nodeName=="fadc_gain") //1
				parseFADCGain(data,node);
			else if(nodeName=="fadc_baseline") //1
				parseFADCBaseline(data,node);
			else if(nodeName=="fadc_delta_t") //1
				parseFADCDeltaT(data,node);
			else if(nodeName=="daq_baseline") //1
				parseDAQBaseline(data,node);
			else if(nodeName=="pmtDiscCal") //1
				parsePMTDiscCal(data,node);
			else if(nodeName=="domid")
				parseMainboardID(data,node);
			else if(nodeName=="date")
				parseDate(data,node);
			else if(nodeName=="time")
				parseTime(data,node);
			else
				errors.addError(mbID,error(WARNING,"Unknown element type '"+nodeName+"'"));
		}
	} catch(xmlParseError& err){
		throw xmlParseError("Failed to parse '"+file+"': "+err.what());
	}
	return(data);
}

void xmlErrorCallback(void* ctx, xmlError* err){
	severity_t severity;
	switch(err->level){
		case XML_ERR_NONE:
		case XML_ERR_WARNING:
			severity = WARNING;
			break;
		case XML_ERR_ERROR:
			severity = ERROR;
			break;
		case XML_ERR_FATAL:
			severity = FATAL_ERROR;
			break;
	}
	static_cast<DOMCalParser*>(ctx)->addError(error(severity,err->line,trim(err->message)));
}

bool extractMainboardID(const std::string& filename, uint64_t& mbID){
	if(filename.size()<19) //needs to be long enough to contain "domcal_" and 12 hex digits
		return(false);
	size_t startIdx=filename.find("domcal_");
	if(startIdx==std::string::npos || (startIdx+12)>=filename.size())
		return(false);
	startIdx+=7; //length of "domcal_"
	std::istringstream idStream(filename.substr(startIdx,12));
	idStream >> std::hex >> mbID;
	return(!idStream.fail());
}

time_t getModificationTime(const std::string& path){
	//TODO: this needs error checking, badly
	struct stat statResults;
	stat(path.c_str(), &statResults);
	return(statResults.st_mtime);
}

void parseAllDOMCalFiles(const std::string& directory, std::map<uint64_t,DOMCalRecord>& data, errorListing& errors){
	DOMCalParser parser(errors);
	
	std::map<uint64_t,time_t> runningTimes;
	for(directory_iterator it(directory), end; it!=end; ++it){
		if(it->path().name()=="." || it->path().name()=="..")
			continue;
		if(is_directory(*it) && it->path().stem()!="histos")
			parseAllDOMCalFiles(it->path().string(),data,errors);
		else if(it->path().extension()=="xml"){
			uint64_t mbID=0;
			if(!extractMainboardID(it->path().stem(),mbID))
				continue;
			try{
				data.insert(std::make_pair(mbID,parser.parseDOMCalFile(it->path().string(),mbID)));
				data[mbID].calTime=getModificationTime(it->path().string());
			} catch(xmlParseError& err){
				std::cerr << "Bleh, uncaught xmlParseError: " << err.what() << std::endl;
			}
		}
		else if(it->path().extension()=="running"){
			uint64_t mbID=0;
			if(!extractMainboardID(it->path().stem(),mbID))
				continue;
			//errors.addError(mbID,error(WARNING,"Calibration may not have completed"));
			runningTimes[mbID]=getModificationTime(it->path().string());
		}
	}
	for(std::map<uint64_t,time_t>::const_iterator runIt=runningTimes.begin(), end=runningTimes.end(); runIt!=end; runIt++)
		data[runIt->first].runningTime=runIt->second;
}

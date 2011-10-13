#include "DOMCalParser.h"

#include <iostream>
#include <sstream>

#include <boost/regex.hpp>

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
			addError(error(ERROR,"Out of range DAC channel number '"+boost::lexical_cast<std::string>(channel)+"'"));
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
			addError(error(ERROR,"Out of range ADC channel number '"+boost::lexical_cast<std::string>(channel)+"'"));
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
		addError(error(ERROR,xmlGetLineNo(b),"Out of range ATWD baseline chip id: "+boost::lexical_cast<std::string>(chip_id)));
		throw xmlParseError("");
	}
	channel = getAttributeWithErrorContext<unsigned int>(b,"channel",error(ERROR,"Invalid ATWD baseline channel number : "));
	if(channel>=3){
		addError(error(ERROR,xmlGetLineNo(b),"Out of range ATWD baseline channel number: "+boost::lexical_cast<std::string>(channel)));
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
		
		//for now, we don't read the actual contents of the histogram
		histo->x_data=NULL;
		histo->y_data=NULL;
		histo->bin_count=0;
		
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
			addError(error(ERROR,xmlGetLineNo(node),"Out of range ATWD amplifier channel number: "+boost::lexical_cast<std::string>(channel)));
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
			addError(error(ERROR,xmlGetLineNo(node),"Out of range ATWD delta t chip id: "+boost::lexical_cast<std::string>(chip_id)));
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
			addError(error(ERROR,xmlGetLineNo(node),"Out of range ATWD delta t chip id: "+boost::lexical_cast<std::string>(chip_id)));
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
		data.hv_gain_calib = callWithErrorContext(readLinearFit,firstChild(node,"fit"),error(ERROR,"Bad transit time calibration: "));
		data.read.HVGainCalib = true;
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
		addError(error(ERROR,xmlGetLineNo(node),"Out of range DAQ baseline chip id: "+boost::lexical_cast<std::string>(chip_id)));
		throw xmlParseError("");
	}
	channel = getAttributeWithErrorContext<unsigned int>(node,"channel",error(ERROR,"Invalid DAQ baseline channel number : "));
	if(channel>=3){
		addError(error(ERROR,xmlGetLineNo(node),"Out of range DAQ baseline channel number: "+boost::lexical_cast<std::string>(channel)));
		throw xmlParseError("");
	}
	bin = getAttributeWithErrorContext<unsigned int>(node,"bin",error(ERROR,"Invalid DAQ baseline bin number : "));
	if(bin>=128){
		addError(error(ERROR,xmlGetLineNo(node),"Out of range DAQ baseline bin number: "+boost::lexical_cast<std::string>(bin)));
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
	const static boost::regex dateRegex("([0-9]{1,2})-([0-9]{1,2})-([0-9]{4})");
	
	try{
		std::string strDate = callWithErrorContext(getNodeContents<std::string>,node,error(ERROR,"Bad date: "));
		boost::smatch rMatch;
		if(!boost::regex_match(strDate, rMatch, dateRegex) || rMatch.size()<4){ //no match
			addError(error(ERROR,xmlGetLineNo(node),"Malformed date: "+strDate));
			return;
		}
		std::string idStr(rMatch[1]);
		std::istringstream ss;
		ss.str(rMatch[1]);
		ss >> data.day;
		ss.clear();
		ss.str(rMatch[2]);
		ss >> data.month;
		ss.clear();
		ss.str(rMatch[3]);
		ss >> data.year;
		data.read.date = true;
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void DOMCalParser::parseTime(DOMCalRecord& data, xmlNode* node){
	const static boost::regex timeRegex("([0-9]{2}):([0-9]{2}):([0-9]{2})");
	
	try{
		std::string strDate = callWithErrorContext(getNodeContents<std::string>,node,error(ERROR,"Bad time: "));
		boost::smatch rMatch;
		if(!boost::regex_match(strDate, rMatch, timeRegex) || rMatch.size()<4){ //no match
			addError(error(ERROR,xmlGetLineNo(node),"Malformed time: "+strDate));
			return;
		}
		std::string idStr(rMatch[1]);
		std::istringstream ss;
		ss.str(rMatch[1]);
		ss >> data.hour;
		ss.clear();
		ss.str(rMatch[2]);
		ss >> data.minute;
		ss.clear();
		ss.str(rMatch[3]);
		ss >> data.second;
		data.read.time = true;
	} catch(xmlParseError& err){
		//stop the error here, assuming it has already been recorded
	}
}

void xmlErrorCallback(void* ctx, xmlError* err);

DOMCalRecord DOMCalParser::parseDOMCalFile(const boost::filesystem::path& file, uint64_t mbID){
	currentMbID = mbID;
	//std::cout << "Reading " << file.string() << std::endl;
	xmlLineNumbersDefault(1); //we want line numbers
	xmlSetStructuredErrorFunc(this,&xmlErrorCallback);
	boost::shared_ptr<xmlDoc> tree(xmlReadFile(file.string().c_str(),NULL,XML_PARSE_RECOVER|XML_PARSE_PEDANTIC),&xmlFreeDoc);
	
	DOMCalRecord data;
	if(!tree){
		errors.addError(mbID,error(FATAL_ERROR,"Failed to parse '"+file.string()+"'"));
		return(data);
	}
	xmlNode* root = xmlDocGetRootElement(tree.get());
	
	try{
		//Ignore DOMCal version
		//data.SetDOMCalVersion(getAttribute<std::string>(root,"version"));
		
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
		throw xmlParseError("Failed to parse '"+file.string()+"': "+err.what());
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

void parseAllDOMCalFiles(const boost::filesystem::path& directory, std::map<int64_t,DOMCalRecord>& data, errorListing& errors){
	const static boost::regex mbIDLocator("domcal_([0-9A-Fa-f]{12})");
	DOMCalParser parser(errors);
	
	for(boost::filesystem::directory_iterator it(directory), end; it!=end; ++it){
		if(is_directory(it->status()) && it->path().stem()!="histos")
			parseAllDOMCalFiles(it->path(),data,errors);
		else if(it->path().extension()==".xml"){
			uint64_t mbID=0;
			boost::smatch rMatch;
			if(!boost::regex_match(it->path().stem(), rMatch, mbIDLocator) || rMatch.size()<2) //no match, so assumedly this is not a DOMCal data file
				continue;
			std::string idStr(rMatch[1]);
			std::istringstream ss(idStr);
			ss >> std::hex >> mbID;
			try{
				data.insert(std::make_pair(mbID,parser.parseDOMCalFile(it->path(),mbID)));
			} catch(xmlParseError& err){
				std::cerr << "Bleh, uncaught xmlParseError: " << err.what() << std::endl;
			}
		}
		else if(it->path().extension()==".running"){
			uint64_t mbID=0;
			boost::smatch rMatch;
			if(!boost::regex_match(it->path().stem(), rMatch, mbIDLocator) || rMatch.size()<2) //no match, so assumedly this is not a DOMCal data file
				continue;
			std::string idStr(rMatch[1]);
			std::istringstream ss(idStr);
			ss >> std::hex >> mbID;
			errors.addError(mbID,error(WARNING,"Calibration may not have completed"));
		}
	}
}
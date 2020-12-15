Summary: IceCube DOMCal Surface Client 
Name: domcal-surface
Version: %{VER}
Release: %{REL}
URL: http://icecube.wisc.edu
Source0: %{name}-%{version}.tgz
License: Copyright 2013 IceCube Collaboration
Group: System Environment/Base
BuildRoot: %{_tmppath}/%{name}-root
Prefix: %{_prefix}

%global debug_package %{nil}

%description
IceCube DOMCal Surface Client

%prep

%setup -q

%build
cd surface; make clean; make DOMCal

%install
install -d ${RPM_BUILD_ROOT}/mnt/data/testdaq
install -d ${RPM_BUILD_ROOT}/mnt/data/testdaq/bin

install surface/DOMCal ${RPM_BUILD_ROOT}/mnt/data/testdaq/bin/
install resources/surface/domcal-wrapper ${RPM_BUILD_ROOT}/mnt/data/testdaq/bin/
install resources/surface/domcal_ic86.config ${RPM_BUILD_ROOT}/mnt/data/testdaq/

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,testdaq,testdaq)
/mnt/data/testdaq/bin/DOMCal
/mnt/data/testdaq/bin/domcal-wrapper
/mnt/data/testdaq/domcal_ic86.config

%changelog
* Tue Dec 15 2020 John Kelley <jkelley@icecube.wisc.edu>
- Make spec file compatible with CentOS8
- Add PCTS DOMs to configuration file
* Thu Apr 16 2020 John Kelley <jkelley@icecube.wisc.edu>
- Config file change (disable 06-59 chip B)
* Tue Mar 17 2020 John Kelley <jkelley@icecube.wisc.edu>
- Config file changes only (added DOMs)
* Wed Feb 14 2018 John Kelley <jkelley@icecube.wisc.edu>
- 8357: Improve surface client dropped DOM handling
* Fri Dec 8 2016 John Kelley <jkelley@icecube.wisc.edu>
- Add DM-ice mainboards to configuration file
* Mon Feb 3 2014 John Kelley <jkelley@icecube.wisc.edu>
- Initial RPM build.



Name:           stupidterm
Version:        %(git describe --tags | sed -ne 's/-/./g;s/^v\(.*\)/\1/p')
Release:        1%{?dist}
Summary:        A Stupid Terminal based on VTE

License:        LGPL
URL:            https://github.com/esmil/stupidterm
#Source0:        https://github.com/esmil/stupidterm/archive/master/stupidterm-master.tar.gz

Requires:       vte291
BuildRequires:  git sed vte291-devel

%description
A Stupid Terminal based on VTE


%define _builddir %(echo $PWD)

%build
make %{?_smp_mflags} clean
make %{?_smp_mflags} release

%install
rm -rf %{buildroot}
%make_install bindir='%{_bindir}'

%files
%{_bindir}/st

%changelog

# Maintainer: Emil Renner Berthing <esmil@mailme.dk>

pkgname=stupidterm
pkgver=1
pkgrel=1
pkgdesc='A Stupid Terminal based on VTE'
arch=('i686' 'x86_64' 'armv5tel' 'armv7l')
url='https://github.com/esmil/stupidterm'
license=('LGPL')
depends=('vte3')
source=()

pkgver() {
  cd "$startdir"
  local ver="$(git describe --tags)"
  ver="${ver#v}"
  echo "${ver//-/.}"
}

build() {
  cd "$startdir"

  make clean
  make release
}

package() {
  cd "$startdir"

  make DESTDIR="$pkgdir" prefix=/usr install
}

# vim:set ts=2 sw=2 et:

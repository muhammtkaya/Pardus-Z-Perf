# Z-Performans derleme ve kurulum. Hedefler: docs/06 bölüm 3.
#   make            servis ve yoklayıcı
#   make test       birim testleri
#   make SAN=1      AddressSanitizer + UBSan ile (geliştirme)
#   sudo make install / uninstall   (PREFIX, DESTDIR desteklenir)
#   make deb        zperf_<sürüm>_amd64.deb
VERSION := 0.1.0
PREFIX  ?= /usr
DESTDIR ?=
CC      ?= gcc
PKGS    := libsystemd json-c

CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wshadow -Wformat=2
CFLAGS  += $(shell pkg-config --cflags $(PKGS))
LDLIBS  += $(shell pkg-config --libs $(PKGS))
ifeq ($(SAN),1)
CFLAGS  += -fsanitize=address,undefined -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address,undefined
endif

DAEMON_SRC := $(wildcard daemon/src/*.c)
DAEMON_OBJ := $(DAEMON_SRC:.c=.o)
LIB_OBJ    := $(filter-out daemon/src/main.o,$(DAEMON_OBJ))
TESTS      := daemon/tests/test_config daemon/tests/test_policy

.PHONY: all test install uninstall deb clean

all: daemon/zperfd tools/probe/probe

daemon/src/%.o: daemon/src/%.c $(wildcard daemon/src/*.h)
	$(CC) $(CFLAGS) -c -o $@ $<

daemon/zperfd: $(DAEMON_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

tools/probe/probe: tools/probe/probe.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

daemon/tests/%: daemon/tests/%.c daemon/tests/test.h $(LIB_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIB_OBJ) $(LDLIBS)

test: $(TESTS)
	@fail=0; for t in $(TESTS); do echo "== $$t"; ./$$t || fail=1; done; exit $$fail

install: all
	install -Dm755 daemon/zperfd             $(DESTDIR)$(PREFIX)/sbin/zperfd
	install -Dm755 scripts/zperf-thaw-all    $(DESTDIR)$(PREFIX)/libexec/zperf/zperf-thaw-all
	install -Dm755 agent/zperf-agent         $(DESTDIR)$(PREFIX)/bin/zperf-agent
	install -Dm755 gui/zperf-gui             $(DESTDIR)$(PREFIX)/bin/zperf-gui
	install -Dm755 cli/zperfctl              $(DESTDIR)$(PREFIX)/bin/zperfctl
	install -Dm644 gui/zperf-gui.desktop     $(DESTDIR)$(PREFIX)/share/applications/zperf-gui.desktop
	install -Dm644 systemd/zperfd.service    $(DESTDIR)$(PREFIX)/lib/systemd/system/zperfd.service
	install -Dm644 polkit/io.github.zperf.policy $(DESTDIR)$(PREFIX)/share/polkit-1/actions/io.github.zperf.policy
	install -Dm644 xdg/zperf-agent.desktop   $(DESTDIR)/etc/xdg/autostart/zperf-agent.desktop
	# Var olan yapılandırmanın üzerine yazma
	test -e $(DESTDIR)/etc/zperf/zperf.conf || install -Dm644 config/zperf.conf $(DESTDIR)/etc/zperf/zperf.conf

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/sbin/zperfd $(DESTDIR)$(PREFIX)/libexec/zperf/zperf-thaw-all
	rm -f $(DESTDIR)$(PREFIX)/bin/zperf-agent $(DESTDIR)$(PREFIX)/bin/zperf-gui $(DESTDIR)$(PREFIX)/bin/zperfctl
	rm -f $(DESTDIR)$(PREFIX)/share/applications/zperf-gui.desktop
	rm -f $(DESTDIR)$(PREFIX)/lib/systemd/system/zperfd.service
	rm -f $(DESTDIR)$(PREFIX)/share/polkit-1/actions/io.github.zperf.policy
	rm -f $(DESTDIR)/etc/xdg/autostart/zperf-agent.desktop
	-rmdir $(DESTDIR)$(PREFIX)/libexec/zperf
	@echo "/etc/zperf/zperf.conf bilerek silinmedi."

deb: all
	rm -rf build/deb
	$(MAKE) install DESTDIR=$(CURDIR)/build/deb
	install -d build/deb/DEBIAN
	sed 's/@VERSION@/$(VERSION)/' packaging/DEBIAN/control > build/deb/DEBIAN/control
	install -m755 packaging/DEBIAN/postinst packaging/DEBIAN/prerm packaging/DEBIAN/postrm build/deb/DEBIAN/
	echo /etc/zperf/zperf.conf > build/deb/DEBIAN/conffiles
	dpkg-deb --root-owner-group --build build/deb zperf_$(VERSION)_amd64.deb

clean:
	rm -f daemon/src/*.o daemon/zperfd tools/probe/probe $(TESTS)
	rm -rf build zperf_*.deb

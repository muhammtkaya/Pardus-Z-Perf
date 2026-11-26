# SPDX-License-Identifier: GPL-3.0-or-later
# Pardus Z-Performans - ust duzey derleme
#
# Kullanim:
#   make            -> zperfd'yi derler (build/zperfd)
#   make prober     -> M1 olceri (build/interaction_prober)
#   sudo make run   -> servisi on planda calistirir (gelistirme/test)
#   make install    -> DESTDIR/PREFIX'e kurar (paketleme icin)
#   make clean      -> derleme ciktilarini siler

CC      ?= gcc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wpedantic
LDFLAGS ?=
# Kodun DERLENMESI icin zorunlu bayraklar: disaridan verilen CFLAGS (ornegin
# dpkg-buildpackage'in sertlestirme bayraklari) bunlari EZMEMELI; aksi halde
# accept4/struct ucred gibi GNU bildirimleri gorunmez ve derleme kirilir.
ZP_CFLAGS := -std=c11 -D_GNU_SOURCE
ALL_CFLAGS = $(ZP_CFLAGS) $(CPPFLAGS) $(CFLAGS)

PREFIX  ?= /usr
DESTDIR ?=

SRCDIR  := src/zperfd
BUILD   := build
SRCS    := $(wildcard $(SRCDIR)/*.c)
OBJS    := $(patsubst $(SRCDIR)/%.c,$(BUILD)/%.o,$(SRCS))
BIN     := $(BUILD)/zperfd
PROBER  := $(BUILD)/interaction_prober

.PHONY: all clean run prober install

all: $(BIN)

$(BIN): $(OBJS)
	@mkdir -p $(BUILD)
	$(CC) $(ALL_CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	@echo "Derlendi: $(BIN)"

$(BUILD)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(BUILD)
	$(CC) $(ALL_CFLAGS) -c -o $@ $<

prober: $(PROBER)
$(PROBER): tests/prober/interaction_prober.c
	@mkdir -p $(BUILD)
	$(CC) $(ALL_CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Derlendi: $(PROBER)"

run: all
	./$(BIN) --debug

# Paketleme/elle kurulum. Python ajan+arayuz /usr/share/zperf altina kurulur;
# /usr/bin'e ince sarmalayici betikler yazilir.
install: all
	install -D -m 0755 $(BIN) $(DESTDIR)$(PREFIX)/sbin/zperfd
	install -D -m 0644 systemd/zperfd.service $(DESTDIR)/lib/systemd/system/zperfd.service
	install -D -m 0644 polkit/org.pardus.zperf.policy \
		$(DESTDIR)$(PREFIX)/share/polkit-1/actions/org.pardus.zperf.policy
	# Masaustu: arayuz menu kaydi + ajanin oturum acilisinda otomatik baslatilmasi
	install -D -m 0644 data/zperf-gui.desktop \
		$(DESTDIR)$(PREFIX)/share/applications/zperf-gui.desktop
	install -D -m 0644 data/zperf-agent.desktop \
		$(DESTDIR)/etc/xdg/autostart/zperf-agent.desktop
	# Profil dosyalari
	install -d $(DESTDIR)/etc/zperf/profiles
	install -m 0644 config/profiles/*.conf $(DESTDIR)/etc/zperf/profiles/
	# Python ajan + arayuz (derlenmis onbellek haric)
	install -d $(DESTDIR)$(PREFIX)/share/zperf
	cp -r src/agent src/gui $(DESTDIR)$(PREFIX)/share/zperf/
	find $(DESTDIR)$(PREFIX)/share/zperf \
		\( -name '__pycache__' -o -name '*.pyc' \) -prune -exec rm -rf {} +
	# Sarmalayici betikler
	install -d $(DESTDIR)$(PREFIX)/bin
	printf '#!/bin/sh\nexec python3 %s/share/zperf/agent/main.py "$$@"\n' "$(PREFIX)" \
		> $(DESTDIR)$(PREFIX)/bin/zperf-agent
	printf '#!/bin/sh\nexport PYTHONPATH=%s/share/zperf/agent\nexec python3 %s/share/zperf/gui/main.py "$$@"\n' \
		"$(PREFIX)" "$(PREFIX)" > $(DESTDIR)$(PREFIX)/bin/zperf-gui
	chmod 0755 $(DESTDIR)$(PREFIX)/bin/zperf-agent $(DESTDIR)$(PREFIX)/bin/zperf-gui
	@echo "Kuruldu -> $(DESTDIR)$(PREFIX)"

clean:
	rm -rf $(BUILD)

/* PSI (Pressure Stall Information): bellek baskısı tetikleyicileri ve ortalamalar.
 * Kaynak: Linux çekirdek belgeleri, accounting/psi. */
#pragma once

#define ZP_PSI_MEMORY "/proc/pressure/memory"

struct psi_avgs {
    double some10, some60;   /* yüzde */
    double full10, full60;
};

/* Tetikleyici kurar ve fd döndürür; poll() ile POLLPRI beklenir.
 * POLLERR gelirse tetikleyici geçersiz olmuştur. */
int psi_trigger_open(const char *spec);

/* /proc/pressure/memory'yi (tetikleyiciden ayrı bir fd ile) okuyup ayrıştırır. */
int psi_read_avgs(struct psi_avgs *out);

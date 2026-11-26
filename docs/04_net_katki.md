# 04 · Projenin Net Katkısı

Aşama 2 değerlendirmesinde "grup tarafından geliştirilen katkı açıkça belirtilmelidir" deniyor ve sözlü sunumda her üyeden kendi katkısını açıklaması bekleniyor. Bu doküman o soruların cevabıdır; rapordaki "Projenin kapsamı ve katkısı" bölümü buradan yazılır.

## Kısa cevap (sunumda ezbere söylenebilecek hali)
> Masaüstü Linux'ta bellek dolunca ya hiçbir şey yapılmıyor ya da bir uygulama öldürülüyor. Sunucu dünyasında Meta, PSI'ye bakarak belleği önceden sıkıştırmanın işe yaradığını gösterdi, ama bu fikir masaüstüne, yani kullanıcının o an hangi uygulamayı kullandığına uyarlanmadı. Biz bunu yaptık: Z-Performans bellek baskısını ölçüyor, kullanıcının önündeki uygulamayı koruyor, arkadakileri öldürmek yerine sıkıştırıp donduruyor ve kullanıcı geri döndüğünde çözüyor.

## Mevcut araçlar ve fark
| Araç | Ne yapar | Z-Performans'tan farkı |
|---|---|---|
| Linux zram, zramctl | Sıkıştırılmış takas aygıtı ve onu kuran komut | Mekanizma; ne zaman neyin sıkıştırılacağına karar vermez. Biz bu mekanizmayı kullanıyoruz. |
| zram-tools, zram-generator | Açılışta sabit ayarlı zram kurar | Statik. Baskıya ve kullanıcının hangi uygulamayı kullandığına göre tepki vermez. **Ölçümde karşılaştırdığımız temel çizgi (C) budur.** |
| earlyoom, nohang | Bellek azalınca en büyük süreci öldürür | Tek tepkileri öldürmek; kaydedilmemiş iş kaybolur. Biz hiç öldürmüyoruz (G-11). |
| systemd-oomd | PSI'ye göre baskı yaratan cgroup'u öldürür | Yine öldürür. XFCE'de tüm uygulamalar tek cgroup'ta olduğu için (B-05) bütün oturumu hedef alabilir. |
| senpai (Meta, TMO) | PSI geri beslemesiyle konteynerlerin bellek sınırını ayarlayıp soğuk sayfaları dışarı iter | Sunucu ve konteynerler için. Kullanıcının odağını bilmez, dondurmaz. **Kademe 1 bu fikrin masaüstü uyarlamasıdır; kodu kullanılmadı.** |
| Android lmkd ve uygulama dondurucu | PSI ile baskıyı izler, arka plandaki uygulamaları dondurur ve gerekirse öldürür | Android'e özgü. Fikir kaynağı; kodu kullanılmadı. |

## Grubun yazdığı şeyler (kod düzeyinde)
| Katkı | Nerede | Kim |
|---|---|---|
| PSI tetikleyicilerine dayalı, olay güdümlü, histerezisli kademeli politika motoru | `daemon/src/psi.c`, `policy.c` | Furkan |
| Uygulamaları systemd geçici scope'larına alma, dondurma, çözme ve servis çökse bile çözme garantisi | `daemon/src/units.c`, `main.c`, systemd birimi | Furkan |
| cgroup bazında proaktif geri kazanım (`memory.reclaim`) ve zram kurulumu (`swapon(2)`) | `daemon/src/cgroupfs.c`, `zram.c` | Furkan |
| Odak, yeni pencere ve ses bilgisini toplayan oturum ajanı | `agent/zperf-agent` | Melih |
| IPC protokolü, polkit yetkilendirmesi ve kullanıcı arayüzü | `daemon/src/ipc.c` (Furkan), `gui/zperf-gui`, `polkit/` (Melih) | İkisi |
| Profiller ve parametre seçimleri (ölçümle gerekçelendirilmiş) | `config/` | İkisi |
| Ölçüm altyapısı: etkileşim yoklayıcısı, toplayıcı, yük betiği, analiz | `tools/` | Yoklayıcı: Furkan; diğerleri: Melih |
| Pardus paketi ve kurulum | `packaging/`, `Makefile` | Melih |
| A–D karşılaştırmalı ölçüm sonuçları | `results/` ve teknik rapor | Melih (yürütme), ikisi (yorum) |

## Grubun yazmadığı, kullandığı şeyler
- **Çekirdek mekanizmaları:** zram, PSI, cgroup v2, `memory.reclaim`, freezer.
- **systemd:** Geçici scope ve dondurma API'leri.
- **Kütüphaneler:**
  - `libsystemd` (sd-bus): systemd ve polkit ile D-Bus üzerinden konuşmak için.
  - `json-c`: IPC mesajları için.
  - GTK 3, PyGObject, libwnck: arayüz ve ajan için.
- **Test aracı:** stress-ng.

Hepsinin lisansı README'de ve raporda listelenir.

## "Bunu zaten zram-tools yapmıyor mu?" sorusuna cevap
Hayır. zram-tools sadece zram'i açar; hangi sayfanın ne zaman sıkıştırılacağını çekirdeğin genel algoritmasına bırakır. Çekirdek, video oynatan uygulamanın sayfası ile yarım saattir dokunulmamış LibreOffice'in sayfası arasında kullanıcı açısından bir fark görmez. Z-Performans bu farkı biliyor:
- Arka plandaki uygulamayı, bellek daha dolmadan önceden sıkıştırıyor.
- Gerekirse dondurarak onun yeniden bellek istemesini de engelliyor.

Bu farkın ölçülebilir karşılığı C ile D arasındaki farktır (M1, M3).

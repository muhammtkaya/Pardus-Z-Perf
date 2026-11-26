# Z-Performans Geliştirme Dokümanları

**Proje:** Pardus Z-Performans: Bellek Baskısına Uyarlanan Bellek Yöneticisi
**Ders:** BİL461 İşletim Sistemleri, Proje Aşama 2
**Grup:** Muhammed Furkan Kaya (231101014), Melih Türker (231101023)
**Nihai teslim:** 7 Aralık 2026 Pazartesi, UZAK sistemi (tek zip). Ardından sözlü sunum.

Bu klasör, Aşama 1 önerisinin teslim edilen son hali (`Pardus_Z_Performans_Proje_Onerisi.pdf`, 17 Kasım) ile Aşama 2 dokümanına (`BIL461-Proje-Asama2.pdf`) göre hazırlandı.

## Hangi doküman ne için?

| # | Doküman | Ne zaman okunur | Sahibi |
|---|---|---|---|
| 01 | [Proje özeti](01_proje_ozeti.md) | İlk gün; raporun ve sunumun giriş metni buradan çıkar | Ortak |
| 02 | [Gereksinimler](02_gereksinimler.md) | Kodlamaya başlamadan; her gereksinimin kimliği (G-xx) testlerde ve checklist'te kullanılır | Ortak |
| 03 | [Kapsam](03_kapsam.md) | İlk gün; Aşama 1'den yapılan değişiklikler burada | Ortak |
| 04 | [Net katkı](04_net_katki.md) | Rapor ve sunum öncesi; "sizin katkınız ne?" sorusunun cevabı | Ortak |
| 05 | [Mimari ve teknik kararlar](05_mimari_ve_teknik_kararlar.md) | Kodlamadan önce; IPC protokolü ve karar kayıtları (TK-xx) burada | Furkan (servis), Melih (ajan, arayüz) |
| 06 | [Kodlama rehberi](06_kodlama_rehberi.md) | Kodlama boyunca; depo yapısı, modüller, kod örnekleri, kurallar | Ortak |
| 07 | [Test planı](07_test_plani.md) | 2. günden itibaren; test kimlikleri (T-xx) ve A–D ölçüm prosedürü | Melih (yürütme), Furkan (servis testleri) |
| 08 | [Riskler ve bulgular](08_riskler_ve_bulgular.md) | Her gün güncellenir; bulgular rapordaki "karşılaşılan problemler" bölümüne gider | Ortak |
| 09 | [Checklist](09_checklist.md) | Her akşam; Aşama 2 teslim şartlarının tamamı burada | Ortak |
| 10 | [İş planı](10_is_plani.md) | Her sabah; 11 günlük takvim ve kapsam daraltma sırası | Ortak |
| 11 | [Teknik rapor planı](11_teknik_rapor_plani.md) | 3 Aralık'tan itibaren; raporun her başlığı ve kaynakları | Ortak |
| 12 | [Demo, video ve sözlü sunum](12_demo_video_ve_sunum.md) | 4 Aralık'tan itibaren; video senaryosu ve olası sorular | Ortak |
| 13 | [README taslağı](13_README_taslagi.md) | Paketleme bitince doldurulur; teslimdeki `README.md` olur | Melih |
| 14 | [Yapay zekâ kullanım kaydı](14_yapay_zeka_kullanim_kaydi.md) | Her kullanımda bir satır; rapordaki zorunlu bölümün kaynağı | Ortak |
| 15 | [Kod iskeletinin durumu](15_iskelet_durumu.md) | Kodlamaya başlarken; hangi dosyada ne hazır, ne TODO, kimde | Ortak |

Daha önce hazırlanan Çalışma Rehberi (`Z_Performans_Calisma_Rehberi.pdf`, depoda değil, proje klasöründe) terim sözlüğü ve elle yapılacak alıştırmalar için hâlâ geçerli.

## Depo ve dallar

- **Depo:** https://github.com/muhammtkaya/Pardus-Z-Perf
- **`Real` dalı:** Projenin yeni kodu bu dalda **sıfırdan** yazılır. Teslim edilecek her şey bu daldan çıkar.
- **`main` dalı:** Önceki bir denemenin kodunu içerir. Bu kod **kullanılmaz** ve `main` dalına dokunulmaz. Teslim zip'i hazırlanırken yanlışlıkla `main`'den dosya alınmamalıdır.
- **Dal yapısı:** `Real` dalının kökü teslim zip'inin yapısını izler: kod `source_code/` altında, bu dokümanlar `docs/` altında. Teslimde `docs/` zip'e konmaz.

## Bu dokümanlardaki varsayımlar

1. **Kod sıfırdan yazılır.** `main`'deki eski kod kullanılmaz; takvim 26 Kasım'dan başlıyor ([İş planı](10_is_plani.md)).
2. **Mousepad alıştırmasının (Çalışma Rehberi, Alıştırma 4) sonuçları henüz bilinmiyor.** [Riskler ve bulgular](08_riskler_ve_bulgular.md) içinde B-08 olarak "doldurulacak" bırakıldı. Mimarideki bir karar (TK-02) bu deneyin sonucuna bağlı; ilk gün yapılmalı.
3. **Sürüm:** Öneride "Pardus 25.0" yazıyor; ön inceleme çıktılarında sanal makineler `VERSION_ID="25.2"` gösterdi. Teslim belgelerinde "Pardus 25 (25.2 üzerinde test edildi)" yazılmalı (bkz. B-09).

## Kimlik kuralları

Dokümanlar arası takip için tutarlı kimlikler kullanılır:

- **G-xx:** gereksinim
- **TK-xx:** teknik karar
- **T-xx:** test
- **R-xx:** risk
- **B-xx:** bulgu
- **M1–M6:** öneride tanımlanan ölçümler
- **A–D:** karşılaştırılan yapılandırmalar

Bir commit, test sonucu ya da rapor paragrafı bir gereksinimi karşılıyorsa o gereksinimin kimliğini anın.

# 14 · Yapay Zekâ Kullanım Kaydı

Aşama 2, bölüm 6: "Yapay zeka araçlarından hangi amaçlarla yararlanıldığını teknik raporda açıklayınız. Her grup üyesinin teslim edilen kodu ... anlayıp açıklayabilmesi beklenmektedir."

Bu kayıt, rapordaki "Kaynaklar ve yapay zekâ kullanımı" bölümünün kaynağıdır. Her kullanımda bir satır eklenir. Tarih ve amaç yeterlidir; konuşmanın tamamı gerekmez.

## Kurallar
1. Yapay zekânın ürettiği hiçbir kod, onu birleştiren kişi tarafından satır satır anlaşılmadan `main` dalına girmez.
2. Yapay zekânın verdiği teknik bilgiler (çekirdek davranışı, API imzaları, lisanslar) birincil kaynaktan doğrulanmadan rapora yazılmaz: çekirdek belgeleri, man sayfaları, Debian telif dosyaları.
3. Yapay zekânın önerdiği bir şey yanlış çıktıysa bu da kaydedilir. Rapora yazmak, araçları eleştirel kullandığınızı gösterir.

## Kayıt

| Tarih | Kim | Araç | Amaç | Ne üretildi / ne yapıldı | Nasıl doğrulandı |
|---|---|---|---|---|---|
| Eki 2026 | Ortak | Gemini | Proje fikri araştırması, ilk öneri taslağı | ZRAM yöneticisi fikri ve ilk metin | Claude ile teknik analiz; birkaç teknik hata bulundu ve düzeltildi (dinamik zram boyutu, sekme uyutma) |
| 12–18 Eki 2026 | Melih | Claude | Fikir listesi, öneri analizi ve revizyonu, çalışma rehberi, ön inceleme betiği, lisans doğrulama | `Pardus-Proje-Fikirleri.pdf`, öneri v2/v3, `Z_Performans_Calisma_Rehberi.pdf`, `on_inceleme.sh` | Betik Pardus 25 GNOME ve XFCE sanal makinelerinde çalıştırıldı; lisanslar Debian telif dosyalarından kontrol edildi |
| 26 Kas 2026 | Melih | Claude | Aşama 2 geliştirme dokümanları | `docs/00`–`14` | Grup tarafından okunacak; kod örnekleri sanal makinede derlenip denenecek |
| | | | | | |

## Rapordaki bölüm için taslak paragraf
> Projede yapay zekâ araçlarından **fikir araştırması** (Gemini), **teknik analiz ve belge hazırlama** (Claude) ve [kodlamada kullanıldıysa: belirli amaçlar, örn. "sd-bus API kullanımına dair örnek kod ve hata ayıklama"] amaçlarıyla yararlanılmıştır.
>
> Yapay zekâ çıktıları doğrudan kullanılmamış; teknik iddialar çekirdek belgeleri, man sayfaları ve Debian telif dosyaları gibi birincil kaynaklardan doğrulanmıştır. Önerinin ilk taslağındaki bazı teknik hatalar bu doğrulama sırasında bulunup düzeltilmiştir: zram boyutunun çalışırken değiştirilebileceği varsayımı ve dışarıdan "sekme uyutma".
>
> Teslim edilen kodun tamamı grup üyeleri tarafından incelenmiş ve anlaşılmıştır. Her üye kendi bileşenini ve diğer üyenin bileşeninin çalışma prensibini açıklayabilir.

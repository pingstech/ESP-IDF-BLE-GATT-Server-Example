# ESP32 ile BLE GATT Sunucu Anlatımı

## Giriş

Bu doküman, ESP32 üzerinde bir **BLE (Bluetooth Low Energy) GATT sunucusunun** nasıl oluşturulacağını adım adım açıklamaktadır. GATT (Generic Attribute Profile), BLE cihazlarının **servisler** ve **özellikler (characteristics)** yoluyla veri alışverişini tanımlayan protokoldür. Bu örnekte ESP32, bir BLE sunucu rolünde çalışarak belirli bir servis ve özellik tanımlar, kendini reklam (**advertising**) paketleriyle duyurur ve bir istemci (örneğin bir akıllı telefon) bağlandığında gelen verileri işler. Özellikle, sunucu gelen JSON formatındaki bir metni alıp yorumlar ve istemciye bir **indication** (onay gerektiren bildirim) ile geri bildirim gönderir. 

Hedef kitle olarak BLE GATT sunucu geliştirmeye yeni başlayanlar ve orta seviye geliştiriciler düşünülmüştür. Bu nedenle her yapı taşı (değişkenler, `#define` sabitleri, `struct` yapıları, fonksiyonlar ve koşullar gibi) sade bir dille ve **Zero to Hero** yaklaşımıyla açıklanacaktır. Örnek kod parçaları ve senaryolarla konular pekiştirilecektir. 

## Gerekli Kütüphaneler ve Temel Tanımlar

BLE GATT sunucu uygulamamıza başlamadan önce, projede kullanılan başlık dosyaları ve temel tanımlara göz atalım:

- **Kütüphane Dahil Edilmeleri:** Uygulama kodunun başında çeşitli kütüphaneler dahil edilir:

  - Standart C kütüphaneleri (`<stdio.h>`, `<string.h>`) ve FreeRTOS çekirdek kütüphaneleri (`freertos/FreeRTOS.h`, `freertos/task.h`, `freertos/queue.h`) temel sistem işlevleri ve RTOS desteği için eklenmiştir.
  - **ESP-IDF Platform Kütüphaneleri:** 
    - `"esp_system.h"` ve `"esp_log.h"` ESP32 sistem işlevleri ve loglama (kaydetme) için, 
    - `"nvs_flash.h"` kalıcı bellek (Non-Volatile Storage - NVS) işlemleri için, 
    - BLE ile ilgili `"esp_bt.h"`, `"esp_bt_main.h"` (Bluetooth kontrolcüyü ve yığınını başlatmak için), 
    - `"esp_gap_ble_api.h"` (GAP: Generic Access Profile API’ları, ör. reklam ve bağlantı işlemleri), 
    - `"esp_gatts_api.h"` ve `"esp_gatt_common_api.h"` (GATT sunucu fonksiyonları ve genel GATT tanımları) dahildir.
    - Ek olarak, `"esp_bt_defs.h"` ve `"esp_bt_device.h"` gibi BLE ile ilgili çeşitli tanım ve yardımcılar dahildir.
  - **JSON İşleme Kütüphanesi:** `"cJSON.h"` kütüphanesi, gelen verinin JSON formatında çözümlenmesi için kullanılmaktadır. Bu kütüphane, C dilinde JSON metinlerini parse etmek (ayıştırmak) ve oluşturmak için kullanılır.
  - **Uygulama Kapsamındaki Başlıklar:** `"app_configs.h"` (genel uygulama yapılandırmaları olabilir) ve `"ble_app.h"` (bu BLE uygulamasına ait fonksiyon bildirimlerini içerir) projeye özeldir.

- **Sabit Tanımlar (`#define`):** 
  - `TAG`: Log çıktıları için kullanılan kısa isim. Kodu incelerken **ESP_LOG** fonksiyonlarında `TAG` kullanıldığını göreceğiz. Burada `"BLE"` olarak tanımlanmıştır. Bu sayede tüm log mesajlarımız konsolda `[BLE]` etiketi ile görünür ve ayırt edilmesi kolay olur.
  - `DEVICE_NAME`: Cihazımızın BLE reklamlarında görünecek adı bu makro ile `"BLE_DEMO"` olarak tanımlanmıştır. İstemciler (ör. telefonunuzdaki bir BLE tarama uygulaması) bu ismi görerek cihaza bağlanabilir.

- **Global Değişkenler:** Uygulamada BLE servis ve bağlantı durumunu takip etmek için birkaç **statik değişken** tanımlanmıştır:
  - `gl_service_handle`: Oluşturulan BLE servisinin tutucu değeri (handle). Servis oluşturulduğunda sistem tarafından atanır ve daha sonra bu servis üzerine özellikler eklerken veya servisi başlatırken kullanılır.
  - `gl_char_handle`: Servis içine eklediğimiz özelliğin (characteristic) handle değeri. Bu değer, özelliği eklendikten sonra atanır ve ileride bu özellik üzerinden veri gönderirken (ör. indication) kullanılır.
  - `gl_conn_id`: Cihaza bağlanan BLE istemcisiyle ilişkilendirilen bağlantı kimliği. Bir istemci bağlandığında sistem tarafından verilir, sunucu o bağlantıya yönelik işlemler (örn. cevap gönderme) yaparken bu kimliği kullanır.

- **Servis UUID Tanımı:** 
  - `service_uuid[16]`: 128-bit uzunluğunda özel bir servis UUID değeri dizisi tanımlanmıştır. BLE protokolünde her servis ve özellik, onları benzersiz tanımlayan UUID (Universally Unique Identifier) değerine sahiptir. Bu örnekte 16 baytlık özel bir servis UUID’si kullanılıyor. Dizi içeriği şu şekildedir:
    ```c
    static uint8_t service_uuid[16] = {
        0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
        0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
    };
    ```
    Bu 128 bit’lik değer, ESP-IDF örneklerinde kullanılan bir temel UUID şablonundan türetilmiştir. Son baytları `...00 00 00 01` olduğu için bu servis için özel kimlik kısmı `0x0001` olarak düşünülebilir. Kısacası, bu servis sunucuda tanımlanırken kendine özgü bir kimliğe sahip olacaktır. (Not: 128 bit’lik özel UUID kullanmak, uygulamanın **özel bir servis** oluşturduğu anlamına gelir. Standart BLE servislerinde genellikle 16 bit’lik kısa UUID’ler kullanılır, ancak özel servisler için 128 bit gerekiyor.)

- **Reklam (Advertising) Veri Yapıları:** ESP32 BLE modülü, reklam paketlerini yapılandırmak için iki önemli yapıyı kullanıyor:
  - `esp_ble_adv_data_t adv_data`: Bu yapı, reklam paketinin içeriğini tanımlıyor. Örnekte statik olarak doldurulmuş:
    - `set_scan_rsp = false`: Bu alan, verilen verinin normal reklam verisi olduğunu, tarama yanıtı (scan response) verisi olmadığını belirtiyor. Yani tek bir reklam paketi kullanacağız.
    - `include_name = true`: Cihaz adının reklam verisine dahil edilmesini sağlıyor (yani `DEVICE_NAME` `"BLE_DEMO"`, reklam paketinde yayınlanacak).
    - `include_txpower = true`: Reklam paketine cihazın yayın gücü bilgisini ekler.
    - `min_interval` ve `max_interval`: 0x0006 ve 0x0010 değerleri ile cihazın tercih ettiği asgari ve azami bağlantı aralığını belirtir. Bu değerler 0x0006 (7.5 ms) ile 0x0010 (20 ms) arasındadır ve istemciye, bağlantı kurulunca kullanılabilecek aralık hakkında bilgi vermek içindir.
    - `appearance = 0x00`: Cihaz için standart bir **görünüm kodu** belirtilmemiş (0 demek, generic - genel bir cihaz). Bu alan, örneğin cihaz bir kalp atış sensörü olsaydı, standart bir görünüm kodu ile belirtilebilirdi.
    - `manufacturer_len` / `p_manufacturer_data`: Üreticiye özgü veri eklenmeyeceği için 0 ve NULL.
    - `service_data_len` / `p_service_data`: Servis verisi eklenmiyor (0 ve NULL).
    - `service_uuid_len` ve `p_service_uuid`: Reklam paketine hangi servis UUID'sinin dahil edileceğini tanımlar. Burada `service_uuid_len = sizeof(service_uuid)` yani 16 baytlık servis UUID'si, ve `p_service_uuid = service_uuid` ile yukarıda tanımladığımız özel servis kimliği pakete ekleniyor. Bu sayede tarama yapan istemci cihazlar, reklam paketi içerisinde bu sunucunun hangi servisi sunduğunu görebilecek.
    - `flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT)`: Bu bayraklar, cihazın **Genel Keşfedilebilir (General Discoverable)** olduğunu ve **BR/EDR (Klasik Bluetooth) desteklemediğini** belirtir. Yani sadece BLE modunda, eşleştirme olmadan bulunabilir bir cihazdır.
  
  - `esp_ble_adv_params_t adv_params`: Bu yapı, reklamın nasıl yapıldığını (zamanlaması ve türü gibi) belirtir:
    - `adv_int_min = 0x20` ve `adv_int_max = 0x40`: Reklam yayın aralığını tanımlar. 0x20 = 32 * 0.625ms ≈ 20ms ve 0x40 = 64 * 0.625ms ≈ 40ms. Yani cihaz yaklaşık 20ms ile 40ms arasında değişen aralıklarla reklam paketi gönderecek (bu oldukça sık, hızlı bir reklam periyodudur).
    - `adv_type = ADV_TYPE_IND`: Reklam türü **ind** (indication) olarak ayarlanmış. ADV_TYPE_IND, genel ve bağlantıya izin veren bir reklam türüdür (connectable undirected advertising). Bu, herhangi bir istemcinin cihaza bağlantı isteği gönderebileceği anlamına gelir.
    - `own_addr_type = BLE_ADDR_TYPE_PUBLIC`: Cihazın kendi BLE adres tipini belirtir. Public, cihazın fabrika atamalı (IEEE tarafından verilen) MAC adresini kullanacağı anlamına gelir.
    - `channel_map = ADV_CHNL_ALL`: Reklamların BLE’in üç reklam kanalında (37, 38, 39) da yapılmasını sağlar.
    - `adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY`: Herhangi bir istemcinin taramasına ve bağlantısına izin ver anlamındadır (yani beyaz liste filtresi kullanılmıyor).

Özetle, bu yapılar cihazımızın **adını ve servis bilgisini içeren bir reklam paketi** hazırlıyor ve bu paketin yaklaşık 20-40ms aralıklarla, tüm kanallarda yayınlanması planlanıyor.

## BLE Modülünün Başlatılması (NVS ve Bluetooth Yığınının İnisyalizasyonu)

BLE GATT sunucu uygulamasını çalıştırmak için önce ESP32’nin BLE modülünü düzgün şekilde başlatmalıyız. Kodda bu işlemler `ble_app_init()` fonksiyonu içerisinde gerçekleştiriliyor. Adım adım inceleyelim:

```c
esp_err_t ble_app_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(0));

    return ESP_OK;
}
```

Bu fonksiyonun yaptığı işlemleri maddeler halinde açıklayalım:

1. **NVS Başlatma:** `nvs_flash_init()` çağrısı ile NVS (Non-Volatile Storage) flash bellek bölümü başlatılır. BLE yığını, eşleşme bilgileri gibi verileri depolamak için NVS kullanabilir. Eğer NVS bölgesinde daha önce kullanılan sayfalar doluysa veya farklı bir versiyon kaldıysa (hata kodu `ESP_ERR_NVS_NO_FREE_PAGES` ya da `ESP_ERR_NVS_NEW_VERSION_FOUND` dönerse), önce `nvs_flash_erase()` ile silinir ve sonra tekrar `nvs_flash_init()` yapılır. Sonuç `ESP_ERROR_CHECK` ile kontrol edilir; bu makro, eğer `ret` değişkeni ESP_OK dışında bir hata kodu içeriyorsa seri porta bir hata logu basar ve programı durdurur. Bu sayede her adımda hataları yakalıyoruz.

2. **Klasik Bluetooth Bellek Serbest Bırakma:** `esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)` çağrısı, klasik Bluetooth (BR/EDR) için ayrılmış denetleyici belleğini serbest bırakır. Uygulamamız sadece BLE modunda çalışacağından, klasik Bluetooth'a ihtiyacımız yok ve bu belleği boşaltarak BLE için daha fazla kaynak bırakıyoruz.

3. **Bluetooth Kontrolcüsünü Başlatma:** `esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();` satırı ile varsayılan kontrolcü yapılandırmasını alıyoruz. Ardından `esp_bt_controller_init(&bt_cfg)` ile Bluetooth kontrolcüsünü başlatıyoruz. Bu denetleyici, ESP32'nin Bluetooth radyo donanımını kontrol eden düşük seviye birimdir. Sonraki adımda `esp_bt_controller_enable(ESP_BT_MODE_BLE)` çağrısıyla denetleyiciyi **BLE modunda** etkinleştiriyoruz. Bu noktada artık Bluetooth radyosu BLE modunda çalışabilir durumdadır.

4. **Bluedroid Yığınını Başlatma:** *Bluedroid*, ESP32'nin Bluetooth host (ana bilgisayar) yığınını ifade eder. Denetleyici alt seviye işlemleri yaparken, Bluedroid üst seviye BLE protokol işlemlerini (GAP, GATT vb.) yönetir. `esp_bluedroid_init()` ile yığın başlatılır ve `esp_bluedroid_enable()` ile çalışır hale getirilir. Bu iki adım tamamlandıktan sonra BLE ile ilgili GAP ve GATT API’larını kullanmaya hazır hale geliriz.

5. **GAP ve GATT Callback Fonksiyonlarının Kaydı:** BLE olayları, programımıza *callback* fonksiyonları aracılığıyla bildirilir. Kodumuzda iki önemli callback tanımladık: `gap_event_handler` (GAP olaylarını işleyecek) ve `gatts_event_handler` (GATT sunucu olaylarını işleyecek). 
   - `esp_ble_gap_register_callback(gap_event_handler)`: GAP olayları meydana geldiğinde `gap_event_handler` fonksiyonumuzun çağrılması için kayıt yapıyoruz. Örneğin, reklam verisi yüklendiğinde veya reklam başlatıldığında bu fonksiyon tetiklenecek.
   - `esp_ble_gatts_register_callback(gatts_event_handler)`: GATT sunucu olayları için kendi fonksiyonumuzu kaydediyoruz. Servis kayıt işlemleri, karakteristik okuma/yazma gibi olaylar bu fonksiyona düşecek.
   
6. **Uygulama Profilinin Kaydı:** `esp_ble_gatts_app_register(0)` çağrısı ile GATT sunucu uygulamamızı BLE yığınına kayıt ettiriyoruz. Burada `0`, uygulama profiline verdiğimiz kimlik (app_id) olarak kullanılmaktadır. Her GATT sunucu uygulamasının bir kimliği olabilir; bu örnek basit olduğu için 0 kullanıyoruz. Bu fonksiyon başarılı olursa, birazdan detaylandıracağımız **ESP_GATTS_REG_EVT** (GATT sunucu kayıt olayı) tetiklenecektir ve bizim `gatts_event_handler` fonksiyonumuz içinde işlenecektir.

7. **Fonksiyon Sonu:** Tüm adımlar sorunsuz geçtiyse `ESP_OK` döndürülür. Bu fonksiyon genellikle `app_main` içinden çağrılmalıdır. Yani `ble_app_init()` çalıştırıldıktan sonra reklam yapma, servis yaratma gibi işlemler arka planda tanımladığımız callback'ler vasıtasıyla gerçekleşmeye başlayacaktır.

Bu başlatma adımları tamamlandığında, BLE modülü çalışır durumdadır ve bir sonraki aşama olan reklam verilerinin yapılandırılması ve servis oluşturma adımlarına geçilebilir.

## Cihaz Adının ve Reklam Verilerinin Ayarlanması

BLE yığını başlatıldıktan sonra, cihazımızın reklam paketlerini hazırlamamız gerekir. Yukarıda **Reklam Veri Yapıları** bölümünde `adv_data` ve `adv_params` yapılarını tanıtmıştık. Şimdi bu yapıların kod içinde nasıl kullanıldığına bakalım.

**Cihaz Adı Ayarı:** Cihazın ismi reklam verisine dahil edilecek şekilde belirlendiği için öncelikle bu ismi BLE yığınına bildirmemiz gerekir. Bu işlem, GATT uygulaması kayıt olduktan sonra gelen olay içinde yapılıyor (ESP_GATTS_REG_EVT – birazdan ayrıntılı bakacağız). Şu satır, kayıt olayında cihaz adını BLE sistemine ayarlar:

```c
esp_ble_gap_set_device_name(DEVICE_NAME);
```

Burada `DEVICE_NAME` bizim `"BLE_DEMO"` olarak tanımladığımız isimdir. Bu çağrı, ESP32 BLE modülüne “reklam yaparken adım olarak BLE_DEMO’yu kullan” demektedir.

**Reklam Verisinin Yapılandırılması:** Cihaz adı ayarından hemen sonra reklam verisini yüklemek için şu fonksiyon çağrılır:

```c
esp_ble_gap_config_adv_data(&adv_data);
```

Bu fonksiyona, önceden hazırladığımız `adv_data` yapısının adresi verilmiştir. `esp_ble_gap_config_adv_data()` fonksiyonu, denetleyiciye reklam paketinin içeriğini iletir. Bu işlem asenkron gerçekleşir; yani fonksiyon çağrıldıktan hemen sonra reklam başlamaz, önce verilerin denetleyiciye yüklenmesi gerekir. Bu yükleme tamamlandığında BLE yığını bir **ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT** olayı üretir. Bu olayı bizim `gap_event_handler` fonksiyonumuz yakalayacak ve reklamı başlatma komutunu o zaman vereceğiz. 

Özetle, cihaz adı ve reklam verisi ayarlandıktan sonra, bunlar denetleyiciye iletilir ve uygun olay geldiğinde reklam yayınları başlatılır. Reklam parametreleri (`adv_params`) ise reklam başlatılırken kullanılacak.

## GAP Olaylarını Yönetme (Advertising ve Bağlantı)

**GAP (Generic Access Profile)**, bir BLE cihazının reklam yapması, bulunması, bağlanması gibi işlemleri yönetir. Uygulamamızda GAP ile ilgili olaylar `gap_event_handler` fonksiyonunda ele alınıyor. Bu fonksiyon bir **switch-case** yapısı içinde farklı olay tiplerine göre işlem yapıyor. Kodda ele alınan başlıca GAP olayları ve yaptıkları şunlardır:

```c
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            ESP_LOGI(TAG, "Advertising started");
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "Advertising stopped");
            break;
        default:
            break;
    }
}
```

- **ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:** Bu olay, reklam verisi denetleyiciye yüklendiğinde meydana gelir. Yukarıda bahsettiğimiz `esp_ble_gap_config_adv_data()` fonksiyonu başarılı olursa, bu event gelir. Handler içinde biz de derhal `esp_ble_gap_start_advertising(&adv_params);` çağrısıyla reklam yayınlarını başlatıyoruz. Burada kullandığımız `adv_params`, reklam periyodu, türü gibi ayarları içeriyordu. Bu çağrı yapıldıktan sonra ESP32 belirlediğimiz parametrelerle BLE reklam paketlerini göndermeye başlar.

- **ESP_GAP_BLE_ADV_START_COMPLETE_EVT:** Reklam başlatma işlemi tamamlandığında (başarılı veya başarısız), bu olay gelir. Kodumuzda eğer `param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS` ise loglamada "Advertising started" mesajı veriliyor. Yani reklamın sorunsuz başladığını bilgi amaçlı kaydediyoruz. (Kodda hata kontrolü doğrudan yapılmamış, ancak bir hata olursa farklı bir status gelecektir. Bu örnekte sadece başarılı senaryo loglanıyor.)

- **ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:** Reklam durdurulduğunda bu olay gelir. Örneğimizde sadece "Advertising stopped" diye log basılıyor. Reklamı durdurmak manuel bir işlem olabilir veya bir bağlantı sonrası otomatik durdurulmuş olabilir. Biz elle durdurmadığımız için bu genelde istemci bağlandığında (ve belki sonrasında bağlantı kopunca) reklamın durduğuna dair bir geri bildirim olabilir. (Bağlantı kurulunca cihaz artık reklam göndermez.)

Diğer GAP ile ilgili olaylar (örneğin tarama yanıtı yükleme gibi) bu basit örnekte kullanılmıyor, `default` kısmında yakalanmadan geçiliyor.

Bu mekanizma sayesinde, BLE sunucu cihazımız program başlatıldığında:
- Reklam verilerini hazırlar ve yükler,
- Yükleme tamamlanır tamamlanmaz reklam yapmaya başlar,
- Başladığını loglar. 

Artık istemci cihazlar "BLE_DEMO" adlı cihazı görebilir ve tanıtım paketindeki 128-bit servis UUID'sini fark edebilirler.

## GATT Sunucu Olaylarını Yönetme (Servis, Özellik ve Veri İşlemleri)

**GATT (Generic Attribute Profile) sunucu olayları**, servis ve özelliklerin oluşturulması, istemci bağlantısı, veri alışverişi gibi konuları kapsar. Kodumuzda bu olayların hepsi `gatts_event_handler` isimli fonksiyonda işlenmektedir. Bu fonksiyon da bir switch-case yapısıyla, farklı **esp_gatts_cb_event_t** türündeki olaylara göre işlem yapıyor. Aşağıda, uygulamamızda ele alınan GATT olayları ve her birinin yaptıklarını detaylı olarak listeliyoruz:

- **ESP_GATTS_REG_EVT (Uygulama Kayıt Olayı):** Bu olay, yukarıda `esp_ble_gatts_app_register(0)` fonksiyonunu çağırdıktan sonra BLE yığını tarafından gönderilir. Sunucu uygulamamızın (app_id = 0) kaydı tamamlandığında tetiklenir. Bu olayı yakaladığımızda yaptığımız işlemler:
  - **Cihaz Adı ve Reklam Verisi:** Önce GAP üzerinden cihaz adını ve reklam verilerini ayarlıyoruz: 
    ```c
    esp_ble_gap_set_device_name(DEVICE_NAME);
    esp_ble_gap_config_adv_data(&adv_data);
    ```
    Bu iki çağrı, bir önceki bölümde anlatıldığı gibi, cihaz adını belirler ve reklam verisini denetleyiciye yollar.
  - **Servis Tanımlama:** Uygulama kaydı tamamlandığı anda artık GATT servislerimizi oluşturmaya başlayabiliriz. Bunun için öncelikle bir servis tanımlayıcısı (ID'si) yapılandırıyoruz:
    ```c
    esp_gatt_srvc_id_t service_id = {
        .is_primary = true,
        .id.inst_id = 0x00,
        .id.uuid.len = ESP_UUID_LEN_128
    };
    memcpy(service_id.id.uuid.uuid.uuid128, service_uuid, 16);
    ```
    Burada `service_id` adlı yapı, oluşturmak istediğimiz servisin özelliklerini içeriyor:
    - `is_primary = true`: Servisimizin bir **birincil servis (primary service)** olduğunu belirtiyoruz. (BLE’de servisler birincil veya ikincil olabilir; birincil servisler temel işlevleri tanımlar.)
    - `id.inst_id = 0x00`: Servisin örnek (instance) ID'si, eğer aynı UUID'den birden fazla servis tanımlanacaksa kullanılır. Bizim tek servisimiz olduğundan 0 yeterli.
    - `id.uuid.len = ESP_UUID_LEN_128`: Bu servis UUID’nin 128 bit uzunlukta olduğunu belirtiyoruz.
    - Ardından `memcpy` ile `service_uuid` dizimizde tanımlı 16 baytlık değeri bu yapının UUID alanına kopyalıyoruz. Sonuç olarak `service_id` yapısı, "primary, inst 0, UUID = (bizim 128-bit UUID)" şeklinde hazırlandı.
  - **Servis Oluşturma:** Şimdi servisi BLE yığınına kaydetmek için:
    ```c
    esp_ble_gatts_create_service(gatts_if, &service_id, 4);
    ```
    fonksiyonunu çağırıyoruz. Burada:
    - `gatts_if`: Bu parametre, bizim GATT arabirimimizi temsil eder. `ESP_GATTS_REG_EVT` olayının handler’ına gelen parametre içinde bize verilmiştir ve o anki uygulama için yığını temsil eder. (Tek bir uygulamamız olduğundan direk kullanıyoruz.)
    - `&service_id`: Oluşturmak istediğimiz servisin tanımını verdiğimiz yapı.
    - `4`: Bu servis içinde ayıracağımız **atribüt handle** sayısı. Sayıyı doğru belirlemek önemli: Her servis kendi içinde bir dizi attribute (özellik ve tanımlayıcılar) barındırır. Bizim servisimizde 1 adet özellik (characteristic) ve o özelliğe ait 1 adet descriptor (aşağıda ekleyeceğiz) olacağını biliyoruz. Bir characteristic eklendiğinde aslında 2 attribute oluşur: biri characteristic bildirimi (declaration), diğeri değer (value) için. Üstüne bir de descriptor ekleyeceğiz. Ayrıca servis kaydının kendisi de bir attribute tutar. Dolayısıyla toplam:
      - Servis tanımı: 1
      - Özellik bildirimi: 1
      - Özellik değeri: 1
      - Özellik tanımlayıcı (CCCD): 1
      Toplam 4 handle yer gerektiği hesaplanır. Bu nedenle create_service fonksiyonuna 4 verdik.
    Servis oluşturma işlemi de asenkron ilerler; fonksiyon çağrıldıktan sonra BLE yığını servis oluşturmayı tamamlayınca bize bir olay gönderecektir: **ESP_GATTS_CREATE_EVT**.

- **ESP_GATTS_CREATE_EVT (Servis Oluşturuldu Olayı):** Bu olay, `esp_ble_gatts_create_service` işlemi tamamlandığında gelir. `param->create.status` ile durumu kontrol edilebilir (başarılı mı diye), ve `param->create.service_handle` alanı, oluşturulan servisin benzersiz handle değerini içerir. Kodumuzda:
  - `gl_service_handle = param->create.service_handle;` ile global değişkene bu handle’ı saklıyoruz. Artık `gl_service_handle` değişkeni üzerinden bu servise ait işlemler yapabiliriz.
  - Sonraki adım olarak servisi etkinleştirmemiz gerekir: 
    ```c
    esp_ble_gatts_start_service(gl_service_handle);
    ```
    Bu çağrı, servisi başlatır ve istemcilerin erişimine açar. Artık servisimiz "yayında" denebilir.
  - **Özellik (Characteristic) Oluşturma:** Servis oluşturulduğuna göre içine bir özellik ekleyebiliriz. Bu örnekte tek bir characteristic ekleyeceğiz. Önce bu özelliğin UUID ve özelliklerini hazırlıyoruz:
    ```c
    esp_bt_uuid_t char_uuid = {
        .len = ESP_UUID_LEN_16,
        .uuid = { .uuid16 = 0xFFE1 },
    };
    esp_gatt_char_prop_t prop = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_INDICATE;
    ```
    - `char_uuid`: Özelliğimizin UUID’si 16 bit uzunluklu (`ESP_UUID_LEN_16`). UUID değerini `.uuid16 = 0xFFE1` olarak verdik. 0xFFE1, bu örnek için seçilmiş rastgele bir özellik kimliği (16 bit’lik UUID'ler genelde özel kullanımlar için 0xFFXX aralığında seçilebiliyor). Bu characteristic, 128-bit servisimizin altında tanımlanacak.
    - `prop`: Özelliğin sahip olduğu **property** (özellik nitelikleri) bit maskesi. BLE characteristic’leri okunabilir, yazılabilir, bildirim/indication gönderebilir vb. özelliklere sahip olabilir. Bizim senaryomuzda bu characteristic iki şeyi desteklemeli: 
      - İstemci bu alana veri **yazabilecek** (Write), 
      - Sunucu da istemciye **indication** gönderebilecek. 
      Bu yüzden `ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_INDICATE` ile iki özelliği birleştirdik. (Indication, Notification’ın onay gerektiren versiyonudur. Biz onaylı bildirimler kullanacağız.)
  - Hazırlıklar tamamlanınca özelliği ekliyoruz:
    ```c
    esp_ble_gatts_add_char(gl_service_handle, &char_uuid,
                           ESP_GATT_PERM_WRITE, prop,
                           NULL, NULL);
    ```
    Bu fonksiyon parametreleriyle:
    - `gl_service_handle`: Hangi servis içine ekleneceği (bizim servisimizin handle’ı).
    - `&char_uuid`: Özelliğin UUID’si.
    - `ESP_GATT_PERM_WRITE`: Attribute için izinler. Burada sadece yazma izni verdik. Yani istemci bu özelliği yazabilir, ancak okuma izni verilmedi (istemci bu özelliği doğrudan okumayacak, zira biz okuma senaryosu yok dedik). Sadece Write ile sınırlayarak bir nebze güvenlik/artırılmış kontrol sağlanabilir.
    - `prop`: Özelliğin bildirdiğimiz property bitleri.
    - Son iki `NULL`: Özel bir başlangıç değeri ve kontrol parametresi belirtmiyoruz. (Eğer characteristic’in başlangıç değeri olsaydı veya büyüklüğü kısıtlaması gibi parametreler tanımlamak isteseydik, bir `esp_attr_value_t` yapısı burada verilebilirdi. Bu örnekte gelen veriyi tamamen dinamik işleyeceğimiz için başlangıç değeri yok.)
    Bu fonksiyon çağrısı da asenkron olup, characteristic eklendiğinde **ESP_GATTS_ADD_CHAR_EVT** olayı gelecektir.

- **ESP_GATTS_ADD_CHAR_EVT (Özellik Eklendi Olayı):** Bu olay, `add_char` işlemi bittiğinde tetiklenir. Eklenen özelliğin bilgileri `param->add_char` altında gelir. Önemli bir alanı `attr_handle`, bu eklenen karakteristiğin handle değeridir. Kodumuzda:
  - `gl_char_handle = param->add_char.attr_handle;` ile global değişkenimize bu handle’ı atıyoruz. Artık `gl_char_handle` üzerinden bu özelliğe göndermeler yapabileceğiz (örneğin istemciye bildirim/indication gönderirken bu handle’ı kullanacağız).
  - **Descriptor (Tanımlayıcı) Ekleme:** Çoğu zaman bir characteristic tek başına yeterlidir, fakat eğer characteristic üzerinden **bildirim (notification) veya indication** göndermek istiyorsak, **Client Characteristic Configuration Descriptor (CCCD)** adlı özel bir descriptor eklemeliyiz. CCCD, BLE standardında UUID değeri 0x2902 olan bir tanımlayıcıdır ve istemcinin bu özellikten bildirim alıp almayacağını kontrol etmesine yarar. İstemci, bu descriptor’a yazma yaparak bildirimi açar veya kapatır. 
    Bizim özelliğimiz Indicate desteklediği için bir CCCD eklemeliyiz. Kodda descriptor ekleme şu şekilde yapılıyor:
    ```c
    esp_bt_uuid_t descr_uuid = {
        .len = ESP_UUID_LEN_16,
        .uuid = { .uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG }  // 0x2902
    };
    esp_ble_gatts_add_char_descr(gl_service_handle, &descr_uuid,
                                 ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                 NULL, NULL);
    ```
    - `descr_uuid`: 16 bit uzunluklu descriptor UUID’si. `ESP_GATT_UUID_CHAR_CLIENT_CONFIG` makrosu 0x2902 değerini içerir, bu da CCCD'nin standart UUID'sidir. 
    - `esp_ble_gatts_add_char_descr`: Bu fonksiyon, mevcut characteristic’e bir descriptor ekler. Parametreleri: Servis handle’ı (hangi serviste olduğunu zaten biliyor), descriptor UUID’si (0x2902), izinler ve isteğe bağlı başlangıç değeri. 
    - İzin olarak `ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE` verdik çünkü istemci bu descriptor’ı okuyup yazabilmeli:
      - Okuma: İstemci, descriptor’ı okuyarak mevcut bildirim/indication durumunu öğrenebilir (açık mı kapalı mı, değer 0x0001/0x0002/0x0000).
      - Yazma: İstemci, bu descriptor’a yazarak bildirimleri açıp kapatabilir.
    - Başlangıç değeri için `NULL` verdik, bu durumda yığın otomatik olarak default değeri atayacaktır. CCCD’nin varsayılan değeri genelde 0x0000’dır (yani bağlantı başlar başlamaz bildirimler kapalı).
    Bu çağrı da asenkron olarak descriptor ekler, tamamlandığında **ESP_GATTS_ADD_CHAR_DESCR_EVT** olayı gelecektir.

    *CCCD nedir?* Kısaca açıklamak gerekirse, **Client Characteristic Configuration Descriptor (CCCD)** bir istemcinin sunucudaki bir özelliğin bildirim/indication gönderme özelliğini kontrol edebilmesi içindir. İstemci bu 2 byte’lık alana yazı yazar:
    - `0x0001` yazılırsa **notification (bildirim)** etkin demektir (eğer özellik notify destekliyorsa).
    - `0x0002` yazılırsa **indication (onaylı bildirim)** etkin demektir (özellik indicate destekliyorsa).
    - `0x0000` yazmak ise her ikisini de devre dışı bırakır. 
    Bizim characteristic sadece Indicate desteklediği için, istemci 0x0002 yazarak sunucudan indication almayı açabilir. CCCD her bağlantı için ayrı ayrı tutulur; bir istemci bağlandığında default olarak 0 (kapalı) başlar, istemci isterse açar.

- **ESP_GATTS_ADD_CHAR_DESCR_EVT (Descriptor Eklendi Olayı):** Descriptor eklendiğinde tetiklenir. Kodda bu olay geldiğinde sadece bir log yazılıyor:
  ```c
  ESP_LOGI(TAG, "Descriptor added. Handle: %d", param->add_char_descr.attr_handle);
  ```
  Bu, eklenen descriptor’ın handle değerini konsola bilgi amaçlı basar. Biz bu handle’ı bir değişkende saklamadık, çünkü çok gerekmedi: CCCD’nin yazılmasını zaten `WRITE_EVT` içerisinde yakalayıp yorumlayacağız. Ancak büyük projelerde descriptor handle’ları da saklanıp yönetilebilir. Bizim örnekte loglamak yeterli görülmüş.

- **ESP_GATTS_CONNECT_EVT (İstemci Bağlandı Olayı):** Bir istemci (ör. telefon) reklamlarımızı görüp cihaza bağlandığında bu olay tetiklenir. `param->connect` içerisinde bağlanan cihaza dair bilgiler bulunur. Kodumuzda:
  ```c
  ESP_LOGI(TAG, "Device connected, conn_id=%d", param->connect.conn_id);
  gl_conn_id = param->connect.conn_id;
  ```
  Basitçe bağlantı kimliğini logluyor ve `gl_conn_id` değişkenine atıyoruz. `conn_id`, BLE yığını tarafından her bağlantıya verilen bir kimlik numarasıdır. Eğer birden fazla istemci aynı anda bağlanabilen bir cihaz olsaydık, her birinin conn_id’si farklı olurdu. (Varsayılan BLE sunucular genelde tek bağlantı destekler, ama ESP32 birden çok BLE bağlantısını da destekleyebilir.) Bu kimliği, belirli bir bağlantıya yönelik veri gönderirken kullanacağız (mesela birden fazla bağlantı olsa, hangisine indication yollayacağımızı bilmek için).

- **ESP_GATTS_WRITE_EVT (İstemci Yazma Yaptı Olayı):** Bu, GATT sunucusu için en önemli olaylardan biridir. İstemci, sunucudaki bir özelliğe veya descriptor’a veri yazdığında tetiklenir. Bizim senaryomuzda istemci iki tür yazma işlemi yapabilir:
  1. **CCCD’ye Yazma (Bildirim Aç/Kapa):** İstemci, özelliğin 0x2902 UUID’li tanımlayıcısına 2 byte’lık bir değer yazarak sunucudan indication almayı açabilir veya kapatabilir. Bu durumda `param->write.len` genelde 2 olur ve `param->write.value` içinde 2 baytlık değer bulunur.
  2. **Özelliğe Yazma (JSON Veri Gönderme):** İstemci, asıl characteristic’imize JSON formatında bir metin yazabilir. Bu durumda yazının uzunluğu gönderilen mesaja bağlı olacaktır (örneğin `{"message":"hello"}` gibi bir string).

  Kodumuz her **WRITE_EVT** aldığında şu işlemleri yapıyor:

  ```c
  ESP_LOGI(TAG, "Write event, handle=%d, len=%d", param->write.handle, param->write.len);
  esp_log_buffer_hex(TAG, param->write.value, param->write.len);
  ```
  İlk satır, gelen yazma olayında hangi attribute’a yazıldığını (`handle`) ve veri uzunluğunu loglar. İkinci satır ise yazılan ham veriyi HEX formatında konsola döker (debug amaçlı). Bu sayede geliştirme sırasında neyin yazıldığını görme şansımız olur.

  Ardından iki ana kontrol yapılıyor:

  - **CCCD Yazma Kontrolü:** 
    ```c
    if (param->write.len == 2 && param->write.value) {
        uint16_t descr_val = param->write.value[1] << 8 | param->write.value[0];
        if (descr_val == 0x0002) {
            ESP_LOGI(TAG, "Indication enabled by client.");
        } else if (descr_val == 0x0000) {
            ESP_LOGI(TAG, "Indication disabled by client.");
        }
    }
    ```
    Burada, eğer yazılan veri 2 bayt uzunluğunda ise bunun CCCD olabileceğini varsayıyoruz. Değerin küçük endian olarak okunmasıyla `descr_val` elde ediliyor (BLE üzerinden gelen değerler genellikle little-endian formatındadır, bu yüzden [1]<<8 | [0] yapıldı). Sonra:
    - Eğer `descr_val == 0x0002` ise istemci indication’ları **etkinleştirmiş** demektir. Konsola "Indication enabled by client." yazıyoruz.
    - Eğer `descr_val == 0x0000` ise istemci **devre dışı bırakmış** demektir, "Indication disabled by client." yazıyoruz.
    Bu kontrol, sadece loglama amaçlı; uygulama içinde başka bir değişkene atama yapmıyoruz. Gerçek bir uygulamada, istemci indication açtıysa sunucu belki bir durum bayrağı tutup sadece o açıkken veri göndermeyi seçebilir. Bizim örnekte, istemci açsa da açmasa da veri gönderme kodu aşağıda yer alıyor – basitlik olsun diye her yazma sonrası sunucu bir yanıt indication’ı gönderecek (istemci açmamışsa bile `esp_ble_gatts_send_indicate` fonksiyonu çağrılacak, ancak istemci tarafında bu paket teslim alınmaz). Normalde, indication göndermeden önce istemcinin gerçekten açtığını kontrol etmek iyi bir pratiktir.

  - **JSON Veri İşleme ve Geri Bildirim:**
    ```c
    if (param->write.len < 200) {
        char json_buf[201] = {0};
        memcpy(json_buf, param->write.value, param->write.len);
        json_buf[param->write.len] = '\0';
        ...
        // JSON parse ve cevap oluşturma
        ...
    }
    ```
    Bu koşul, gelen verinin uzunluğunu kontrol ediyor. 200 karakterden küçükse işleme alınıyor (200 bayttan uzun bir JSON beklemiyoruz; bir üst limit koymak bellek açısından iyi). 
    - Önce `json_buf` adında bir tampon tanımlanıyor ve tüm içerik oraya kopyalanıyor, ardından null karakter `'\0'` eklenerek C string formatına getiriliyor. Bu sayede BLE üzerinden gelmiş ham baytlar, rahatça işlenebilecek bir C dizesi haline geliyor.
    - Sonraki adım JSON parse (ayıştırma) işlemidir:
      ```c
      cJSON *root = cJSON_Parse(json_buf);
      if (root) {
          cJSON *msg_item = cJSON_GetObjectItem(root, "message");
          if (cJSON_IsString(msg_item)) {
              char response[128];
              snprintf(response, sizeof(response), "{\"ack\":\"%s\"}", msg_item->valuestring);
              esp_ble_gatts_send_indicate(
                  gatts_if,
                  param->write.conn_id,
                  gl_char_handle,
                  strlen(response),
                  (uint8_t *)response,
                  true
              );
          } else {
              const char *err = "{\"error\":\"missing_message\"}";
              esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id,
                                          gl_char_handle, strlen(err),
                                          (uint8_t *)err, true);
          }
          cJSON_Delete(root);
      } else {
          const char *err = "{\"error\":\"invalid_json\"}";
          ESP_LOGW(TAG, "Invalid JSON received: %s", json_buf);
          esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id,
                                      gl_char_handle, strlen(err),
                                      (uint8_t *)err, true);
      }
      ```
      Bu kod parçasını adım adım inceleyelim:
      - `cJSON_Parse(json_buf)`: Bu fonksiyon `json_buf` içindeki metni JSON yapısına çevirmeye çalışıyor. Dize geçerli bir JSON ise `root` adında bir cJSON nesnesi döner, değilse `NULL` döner.
      - `if (root)`: JSON geçerliyse içeri gireceğiz. İlk iş olarak `cJSON_GetObjectItem(root, "message")` ile JSON objesinden `"message"` anahtarına karşılık gelen değeri arıyoruz. Bu örnekte, istemciden gelen JSON’un `{"message": "..."} ` formatında olması bekleniyor.
      - `if (cJSON_IsString(msg_item))`: Bulunan "message" değeri gerçekten string tipinde mi kontrol ediyoruz. Beklentimiz, `"message"` anahtarının değerinin bir metin olması (örneğin `"selam"` gibi). Eğer string ise:
        - `msg_item->valuestring` ifadesi, cJSON kütüphanesinin bize sağladığı, ilgili JSON alanının C string karşılığını verir. Örneğin istemci `{"message":"selam"}` gönderdiyse, `msg_item->valuestring` `"selam"` içerir.
        - Bir cevap oluşturmak için `response` adında bir karakter dizisi hazırlıyoruz ve `snprintf` kullanarak `{"ack":"selam"}` gibi bir JSON string oluşturuyoruz. Yani gelen mesajı aynen `"ack"` alanı altında geri göndereceğiz. Bu, basit bir **alındı onayı (acknowledgement)** mekanizmasıdır.
        - Sonra `esp_ble_gatts_send_indicate(...)` fonksiyonunu çağırıyoruz. Bu fonksiyon sunucudan istemciye bir characteristic değeri gönderir. Parametreleri:
          - `gatts_if`: GATT arayüzümüz (bu, event handler'a gelen parametrelerde de vardı, hangi uygulama üzerinden gönderileceğini belirtiyor).
          - `param->write.conn_id`: Yazma işlemini yapan bağlantının kimliği; biz `ESP_GATTS_CONNECT_EVT`de bunu `gl_conn_id`ye kaydetmiştik, fakat burada parametre içinde de mevcut, doğrudan kullanıyoruz.
          - `gl_char_handle`: Hangi attribute’un gönderildiğini belirtmek için, bizim characteristic’in handle değeri. Indication, belli bir characteristic’e ait bir değer gibi gönderildiği için o char’ın handle’ını veriyoruz ki karşı taraf hangi özellikten geldiğini anlasın.
          - `strlen(response)`: Gönderilecek verinin uzunluğu (JSON stringimizin uzunluğu).
          - `(uint8_t *)response`: Gönderilecek veri byte dizisi olarak. Burada bizim response bir char dizisi ama onu uint8_t pointerına dönüştürdük (C dilinde bu casting gerekli olabilir).
          - `true`: Bu argüman bildirim tipini belirtir. `true` demek **indication (onaylı bildirim)** gönderiyoruz (istemci, alındığını BLE protokolüyle onaylayacak). Eğer `false` olsaydı **notification (onaysız bildirim)** olurdu.
        - Bu fonksiyon çağrıldığında, eğer istemci tarafında CCCD ile indication açılmışsa, istemci bu veriyi alacak ve otomatik olarak bir onay paketi (ACK) geri gönderecek (BLE protokol düzeyinde). 
      - Eğer `"message"` alanı JSON içinde yoksa veya string tipinde değilse (`else` bloğu):
        - `const char *err = "{\"error\":\"missing_message\"}";` ile küçük bir hata cevabı tanımlıyoruz. Bu durumda, JSON geçerli ama beklenen "message" anahtarı bulunamadı ya da değeri string değildi. 
        - `esp_ble_gatts_send_indicate` ile yine benzer şekilde bu hata mesajını gönderiyoruz. Bu sefer içerik `{"error":"missing_message"}` olacak.
      - `cJSON_Delete(root)`: JSON nesnesini kullanıp işimiz bitince bellekten silerek temizliyoruz.
      - `else { ... }`: JSON parse başarısız olursa (yani `root == NULL` döndüyse):
        - Bu durumda gelen metin geçerli bir JSON formatında değil demektir. `ESP_LOGW` ile bir uyarı logu basıyoruz ve hatalı JSON’ı da logluyoruz.
        - Sonra başka bir hata yanıtı hazırlıyoruz: `{"error":"invalid_json"}`. Bunu da indication olarak istemciye gönderiyoruz.
      Bu şekilde, istemciden gelen mesaja göre üç farklı senaryo kapsandı:
        1. Doğru format ve "message" içeriyor → `"ack": "<içerik>"`
        2. JSON formatı doğru ama "message" alanı yok/yanlış → `"error":"missing_message"`
        3. JSON formatı geçersiz → `"error":"invalid_json"`
      Sunucu her durumda bir yanıt indication göndermeye çalışıyor. (Gerçek kullanımda, istemci CCCD’yi açmamışsa bu indication’ı almaz; biz burada basit tutuyoruz ve gönderiyoruz.)
  
  - **Write Rsp (Yazma Yanıtı):** Son olarak, yazma işlemlerinin protokol gereği bir cevaba ihtiyacı olabilir:
    ```c
    if (param->write.need_rsp) {
        esp_ble_gatts_send_response(gatts_if, 
                                    param->write.conn_id,
                                    param->write.trans_id,
                                    ESP_GATT_OK, NULL);
    }
    ```
    `param->write.need_rsp` alanı, istemcinin bu yazma işlemi için bir alt seviye protokol cevabı bekleyip beklemediğini belirtir. BLE’da iki türlü yazma vardır: **Write Command** (yanıt gerekmez, hızlıdır) ve **Write Request** (yanıt bekler). Eğer istemci Write Request kullandıysa, sunucu bu fonksiyonla bir yanıt göndermelidir, aksi takdirde bağlantı protokol akışı takılır. 
    Biz, koşul sağlanırsa `esp_ble_gatts_send_response` ile standart bir yanıt gönderiyoruz:
    - `gatts_if`, `conn_id` bilindik parametreler,
    - `param->write.trans_id`: İşlem ID’si, BLE protokolünün o yazma işlemini tanımlayan geçici kimliği, bunu yanıtla eşleştirmek için geri gönderiyoruz.
    - `ESP_GATT_OK`: Sonuç kodu (işlemin başarıyla gerçekleştiğini belirtiyoruz).
    - `NULL`: Eğer geri dönen bir veri olsaydı burada bir `esp_gatt_rsp_t` yapısı verilebilirdi (örneğin bir okuma isteğine cevapta değer döndürmek için). Yazma için genellikle sadece status yeterlidir, veri döndürmüyoruz.
    Bu yanıt işlemi, istemciye alt seviye "yazma işlemin başarıyla alındı" bilgisini verir. Zaten biz esas işlevsel cevabı (ack veya error JSON) yukarıda indication olarak gönderdik. Fakat BLE protokol gereği bu response’u da unutmamak gerekir.

  Özetle, WRITE_EVT içinde hem kontrol descriptor’ının yazılması (CCCD) ele alınıyor hem de eğer gelen veri bir mesaj ise JSON olarak değerlendirilip yanıtlanıyor. Bu bölüm, uygulamanın istemciyle esas etkileşime girdiği kısım olduğundan özellikle önemli. 

- **ESP_GATTS_DISCONNECT_EVT (Bağlantı Kesildi Olayı):** Bağlı olan istemci cihaz bağlantıyı kopardığında bu olay gelir. Kodumuz:
  ```c
  ESP_LOGI(TAG, "Device disconnected");
  esp_ble_gap_start_advertising(&adv_params);
  ```
  Sadece log basıyor ve ardından tekrar `esp_ble_gap_start_advertising(&adv_params)` ile reklamları başlatıyor. Bu sayede cihazımız yeniden keşfedilebilir hale geliyor ve başka bir istemci (ya da aynı istemci) bağlanmak isterse hazır oluyor. BLE cihazları tipik olarak bağlantı koptuğunda tekrar advertising moduna dönerler, biz de bunu yapmış olduk.

- **Diğer Olaylar (default):** Yukarıda listelenenler haricinde bir GATT olayı gelirse (örneğin BLE yığını içinde ilgilenmediğimiz başka bir durum), `default` bloğu sayesinde hiçbir işlem yapmadan geçiyoruz.

Bu olay yönetimlerinin hepsi bir araya gelerek uygulamanın genel akışını oluşturur:
1. Uygulama kaydolur (REG_EVT) → Cihaz adı, reklam verisi hazırlanır, servis oluşturma başlatılır.
2. Servis oluşturulur (CREATE_EVT) → Servis başlatılır, characteristic eklenir.
3. Characteristic eklendi (ADD_CHAR_EVT) → Handle saklanır, CCCD eklenir.
4. Descriptor eklendi (ADD_CHAR_DESCR_EVT) → (Sadece log).
5. İstemci bağlanır (CONNECT_EVT) → Bağlantı kimliği saklanır.
6. İstemci veri yazar (WRITE_EVT) → Eğer CCCD ise loglama; eğer mesaj ise JSON parse + ack/error indication; ve gerekli protokol yanıtı.
7. İstemci bağlantıyı keser (DISCONNECT_EVT) → Tekrar reklam moduna dönülür.

Bu sıralama, tipik bir BLE GATT sunucu yaşam döngüsüdür. Her adımda yapılan işlemler ile cihazımız fonksiyonel bir şekilde istemciyle haberleşebilir duruma gelir.

## Uygulamayı Test Etme ve Çalışma Doğrulaması

Artık kodun nasıl çalıştığını anladığımıza göre, bu BLE GATT sunucu örneğini nasıl test edebileceğimize bakalım. Aşağıda, uygulamayı gerçek bir senaryoda denemek için izlenebilecek adımlar sıralanmıştır:

1. **Donanımı Hazırlayın:** ESP32 geliştirme kartınızı bilgisayarınıza bağlayın ve yukarıda açıklanan kodu derleyip yükleyin. Cihaz yeniden başlatıldığında BLE modülü başlatılacak ve cihaz reklam yapmaya başlayacaktır.

2. **Bir BLE İstemci Uygulaması Edinin:** Akıllı telefonunuza NRF Connect for Mobile veya LightBlue Explorer gibi bir BLE tarama ve test uygulaması yükleyin. Bu tür uygulamalar, yakındaki BLE cihazlarını taramak ve servis/özelliklerini keşfetmek için kullanışlıdır.

3. **Cihaza Bağlanın:** Uygulamayı açıp tarama yaptığınızda **"BLE_DEMO"** isimli cihazı görmelisiniz. Bu, ESP32’nizin reklam paketindeki isimdir. Cihaza dokunarak/seçerek bağlanın. Bağlanınca ESP32 konsolunda "Device connected" mesajı görünecektir.

4. **Servis ve Özelliği Bulun:** Bağlandıktan sonra istemci uygulamanız, cihazın servislerini keşfetmelidir. Bu örnekte bir adet özel servisimiz var (UUID’si 128-bit olan). Servis altında bir adet karakteristik (UUID 0xFFE1) ve onun bir adet descriptor’ı (UUID 0x2902, yani CCCD) bulunacaktır. NRF Connect gibi uygulamalarda genelde servis ve özellikler listelenir, 0xFFE1 özelliğini ve altında "Client Characteristic Configuration" descriptor’ını görebilirsiniz.

5. **Indication’ı Etkinleştirin:** İstemci uygulamada 0xFFE1 karakteristiğinin özelliklerine bakın. "Indicate" desteği olduğunu göreceksiniz. Indication alabilmek için öncelikle istemci tarafında bu özelliği etkinleştirmek gerekir. NRF Connect’te genelde characteristic üzerine tıklayınca "Enable Indication" seçeneği çıkar (veya CCCD değerine 0x0002 yazmak şeklinde bir arayüz olur). Bunu yaptığınızda:
   - Telefon uygulaması ESP32’ye CCCD’yi yazacak (0x02,0x00). Bu bizim cihazımızda WRITE_EVT tetikleyecek; kodumuz "Indication enabled by client." diye log basacaktır.
   - Artık ESP32’nin bu karakteristik için indication göndermesine izin verilmiş oldu.

6. **JSON Mesajı Gönderin:** İstemci uygulama üzerinden 0xFFE1 karakteristiğine bir yazma işlemi yapacağız. Uygulamada "Write value" veya benzeri bir seçenekle, metin girişi yapabileceğiniz bir dialog açılır. Bu alana örneğin:  
   ```
   {"message":"hello"}
   ``` 
   yazın ve gönderin. Bu JSON metni BLE üzerinden ESP32’ye iletilecektir. 
   - ESP32 tarafında WRITE_EVT işleyecek, JSON parse başarılı olacak, `"message"` anahtarını bulacak (`"hello"` değerini), ve `"ack":"hello"` şeklinde bir JSON yanıt hazırlayıp `esp_ble_gatts_send_indicate` ile istemciye gönderecektir.
   - Telefon uygulamanızda, gönderdiğiniz değere karşılık sunucudan gelen bir bildirim göreceksiniz. Genellikle "Notification/Indication received" gibi bir log veya arayüzde karakteristik değeri değişimi olarak görünür. İçeriğin `{"ack":"hello"}` olduğunu teyit edin.
   - Bu, iletişimin başarıyla gerçekleştiğini gösterir: İstemci `"hello"` mesajını gönderdi, sunucu bunu aldı ve `"ack":"hello"` diyerek geri bildirim yaptı.

7. **Farklı Durumları Deneyin:** 
   - Örneğin, geçersiz bir JSON gönderin: `{"msg":"hi"}` veya tamamen bozuk bir metin gibi. 
     - Eğer JSON formatı bozuksa (örneğin `{message:hello}` gibi tırnakları eksik veya sözdizimi hatalı bir metin), ESP32 konsolunda "Invalid JSON received" uyarısı loglanacak ve istemciye `{"error":"invalid_json"}` şeklinde bir indication gönderilecektir.
     - Eğer JSON formatı geçerli fakat `"message"` anahtarını içermiyorsa (yukarıdaki örnekte "msg" gönderdik, beklenen "message" yok), bu durumda ESP32 `{"error":"missing_message"}` yanıtını gönderecektir.
   - Telefon uygulamasında bu hata mesajlarını da alıp almadığınızı gözlemleyin. Bu sayede sunucunun farklı girişlere nasıl tepki verdiğini test etmiş olursunuz.

8. **Bağlantıyı Kesin:** İstemci uygulamada bağlantıyı sonlandırın. ESP32 konsolunda "Device disconnected" logu çıktığını göreceksiniz. Ayrıca kodumuz otomatik olarak `esp_ble_gap_start_advertising` çağırdığından, cihaz tekrar reklam moduna döner. Bunu, telefon uygulamasında tekrar tarama yaparak "BLE_DEMO" cihazının yeniden görünür olmasından anlayabilirsiniz. Artık başka bir bağlantı kurulabilir.

Bu test senaryosu, uygulamamızın tüm parçalarının bir arada çalıştığını doğrular: reklam yayını, bağlantı kurulması, CCCD ile indication kontrolü, veri alışverişi ve cevap verme, bağlantı kopunca yeniden reklam vb. Hepsi beklendiği gibi işlemişse, **ESP32 üzerinde basit bir BLE GATT sunucusunu başarıyla geliştirdiniz** demektir.

## Sonuç

Bu dokümanda, **ESP32 BLE GATT sunucu örneğini** ayrıntılı bir şekilde ele aldık. Sıfırdan başlayarak her adımı açıkladık: BLE modülünün başlatılmasından servis ve özelliklerin tanımlanmasına, reklam (advertising) mekanizmasından istemci ile veri alışverişine kadar tüm önemli bölümleri inceledik. Özellikle JSON veri işleme ve istemciye indication ile geri bildirim gönderme gibi konulara değinerek, gerçek bir uygulama senaryosuna uygun basit bir protokol oluşturduk. 

Bu **Zero to Hero** yaklaşımındaki açıklamalarla, BLE GATT sunucusunun temelini kavradığınızı umuyoruz. Artık bu örnek üzerinde değişiklikler yaparak daha ileri özellikler ekleyebilirsiniz:
- Farklı bir UUID ile ek özellikler (characteristics) tanımlamak,
- Okunabilir özellikler ekleyip `ESP_GATTS_READ_EVT` olayını işlemek,
- Birden fazla hizmet (service) oluşturmak,
- Gelen veriyi gerçek uygulama mantığına göre işlemek (örneğin bir LED yakma/söndürme komutu alıp fiziksel olarak bir pin toggling yapmak),
- veya güvenlik (şifreleme/eşleşme) özelliklerini incelemek gibi.

ESP32’nin esnek BLE API’ları sayesinde, çok çeşitli BLE cihaz senaryolarını gerçekleştirmek mümkün. Bu dokümandaki örnek, bu yolda sağlam bir başlangıç olacaktır. Artık elinizde çalışan bir temel varken, onu kendi proje ihtiyaçlarınıza göre özelleştirmeye başlayabilirsiniz. Başarılar ve iyi kodlamalar!
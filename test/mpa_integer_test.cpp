/*
    COPYRIGHT: David Geis 2025
    LICENSE:   MIT
    CONTACT:   davidgeis@web.de
*/

#include <assert.h>

#include "mpa_integer.h"

template <typename word_t>
void test_integer()
{
    using Integer = MPA::Integer<word_t>;
    const Integer zero(0), one(1), x1("0xab123567567adeeff143565756742"), x2("0x1234aeefdbba123231221"), x3("0xde");

    // basic operations
    assert(zero << 1234567U == zero);
    assert(Integer("0x100000000000000000000000000000001") - Integer("0x200000000000000000000000000000000") ==
           Integer("-0xffffffffffffffffffffffffffffffff"));
    assert(x1 + x2 == Integer("0xab12356768af8ddfccfd688987963"));
    assert(x1 - x2 == Integer("0xab123567444630001589442525521"));
    assert(x1 * x2 == Integer("0xc2a7c5ac63337f66e25d95378627825a8c7f66eb1796af382"));
    assert(x1 / x2 == Integer("0x96582653d"));
    assert(x2 / x1 == zero && -x1 / x1 == -one && x1 - x1 == zero && x2 - x2 == zero && x3 - x3 == zero);
    assert(x1 % x2 == Integer("0x5dfeca967ad3f6311065"));
    assert(one << 1232 ==
           Integer("0x100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                   "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                   "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                   "00000000000000000000000000000000000"));
    assert(Integer("-0x725154a39b3442a17a736a8d4ca728ae48e0b77a13f070eaa59ffa2d04") +
               Integer("0x14b737143e25d93660ed6f8dda4a59f44fc4ea841") ==
           Integer("-0x725154a39b3442a17927f71c08c4cb1ae2d1e081364bcb4b60a3ab84c3"));
    assert((-x1 + x2) * (x1 + x2) == -MPA::power(x1, 2) + x2 * x2);
    assert(-(-x1 + x2 + x3) * (x1 + x2 + x3) == MPA::power(x1, 2) - MPA::power(x2 + x3, 2));
    assert(MPA::gcd(x1, x2) == one);
    assert(MPA::lcm(x1, x2) == x1 * x2);
    {
        Integer r, s, t;
        MPA::egcd(x1, x2, &r, &s, &t);
        assert(s * x1 + t * x2 == one && r == one);
    }
    assert(MPA::gcd(x3 * x1, x3 * x2) == x3);
    assert(MPA::lcm(x3 * x1, x3 * x2) == x3 * x1 * x2);
    {
        Integer r, s, t;
        MPA::egcd(x3 * x1, x3 * x2, &r, &s, &t);
        assert(s * x1 * x3 + t * x2 * x3 == x3 && x3 == r);
    }
    assert((-one) << 1232 ==
           Integer("-0x100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                   "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                   "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                   "00000000000000000000000000000000000"));
    assert((Integer("0xdead234346345643123122229000123123bbedeaadfeebc1231221") >> 125) ==
           Integer("0x6f5691a1a31a2b218918911"));
    assert((((-one) << 1232) >> 1232) == -one);
    assert(Integer("0x37d213e1476501c79731236054fddd8ebd7872b0046d01d1648a0efc6a3add5559"
                   "542c09a02ed61c8489797be90965a0c9160872c7ff862dcd00865c3c6a6edab1ad14"
                   "c5dcf04e124a3fb6cdf1ba9ccfa3ca3da5ce0ea2de7a61c6bc2872a659db49e4aee6"
                   "0362e6a4fba2642c2d479e97074d47c297dea48af0870eb40702b9aae74904c2c232"
                   "ce8") -
               Integer("0x37d213e1476501bd1b464ae796b3db453a895d562e9d287180fb90ebaf90c0"
                       "3f8608d8fd0ab4fc5fc468dab4b5cdc5df2fc2cb1c91e78808a4000000000000"
                       "0000000000000000000000000000000000000000000000000000000000000000"
                       "0000000000000000000000000000000000000000000000000000000000000000"
                       "0000000000000000000") ==
           Integer("0xa7bead878be4a024982ef1559d5cfd95fe38e7e10baaa1d15d34b530c9579d9bcc"
                   "0209ec7333b9fc199533d563617fe252900865c3c6a6edab1ad14c5dcf04e124a3fb"
                   "6cdf1ba9ccfa3ca3da5ce0ea2de7a61c6bc2872a659db49e4aee60362e6a4fba2642"
                   "c2d479e97074d47c297dea48af0870eb40702b9aae74904c2c232ce8"));
    assert((one << 1250) * (Integer(5) << 1000) ==
           Integer("10386285306507684978916116673548282354283347549216353826038334983480"
                   "27201818349942027387547467378873904592464517625786136825938489506205"
                   "92755941557106829032084265303181935596507560999120425528567022380661"
                   "23572162548750879051043747647080529772091348678096826144703259828444"
                   "47784542971143877373701961751402593647945496679770694654679196072178"
                   "02872556814026437164802552462840470678546798223064738515064877124832"
                   "12368580011577701514625612479747067924533519120576415727619084496301"
                   "05942259278332874668796346706653448200081400989389809848728879382742"
                   "94460824212590209039628262379472087788409838407159209572950998901949"
                   "019881910422809583427242154239313892345918998357290442046740365312"
                   "0"));
    assert((one << 4550) * (Integer(5) << 2330) ==
           Integer("61001452000887999186066317097615142460822331745139925945796720080769"
                   "49576967582067251719519417787212797086453124394851020251317526151338"
                   "49224403137719579538425726614404773805288202734155038588755244374887"
                   "30204562702411019589393455097780527723212599670441303162788949463505"
                   "87924285003623217715637438595344285604377323212193968029123243188034"
                   "69185435565961752737453248169737036110076943062493345324737310711781"
                   "93676336997078183631986317270082490886793153081515888185914709636317"
                   "28223379723319193429897397022357152552952075077818150207867441309134"
                   "42176734066190118983497612453917229701428847796812643185222584353235"
                   "94959726668228236428456833694318014836063519388899213088787849524472"
                   "22039427490598860840962631056428328792970508673717010436287936735802"
                   "80098233550514235296767738807399473037641376931967783541069501834662"
                   "72773128650070340912481860747931807513146529592592351982433663571192"
                   "80201903163482244540999205829125727216090491575288429689845594995373"
                   "06512501623489417102089875269793866647074064757037534683449246214685"
                   "56933499945991858858479061909226504901503103240132126227179362769455"
                   "83109154816318594148713435523569023918895081485716342656980554380593"
                   "22059246262891346700904664238605584965911255063045174710555306853858"
                   "36132224244766321215233477102307268235645888228731193848430438070316"
                   "12190421510218714695870070392675372191414948232341401726798151988173"
                   "18350768247250094019748186441180951934492101165375926059802179980927"
                   "99208275125297573621161961292661252866712059295435446584900054140928"
                   "16634381938251214101707558521736056919798387900109049389561576153888"
                   "30796192333761572347719545040606643535865645853939240515412908925241"
                   "18085154863155104257568792987364052374610841531073448397674416257774"
                   "88657031972939241121888979546341864730373371878051664330166171709687"
                   "89916799873918869262158595760132470012854260206258278089161940120119"
                   "67897384735422960784686705291343210557783834659308014109571223748579"
                   "14566288377167555249922074996519618678363672833584529505532150745342"
                   "04334981858896410009441242283099620241597575971035892434552060692306"
                   "74124797557151291681931138170880"));
    assert((Integer(5) << 2330) * (Integer(5) << 2330) ==
           Integer("15765937581418552934153156722051900011763197750935255435705560423710"
                   "35962461446112200597963560972019693595259732637834136876740648523644"
                   "22959413583946417243619734297193772034259220297265561881025792906389"
                   "65082164798843098007647530064978179449819451335173188928904481339031"
                   "41652143828294677160789455820984548005744282756046016010067933056711"
                   "89392691549639441297068184624796295310460564520413301920705030928392"
                   "45417204101241237787658404745521555677169420670925655747745430040179"
                   "66155689409895690519382361476753369410927303819938251965864844960942"
                   "73820483678459151726017378992212757939201535844586121099879466705333"
                   "48428091631901159154126605846937015210566585913648531728423522313248"
                   "63694875246053065288132087123580495502619780570781771769235447690016"
                   "56436212444813495695199383619879949795453228217207588586606379358887"
                   "60727042181905714167281579318375964424100821479660093919050876802673"
                   "09010642239161437494089551746566388178230836650430341543298388773215"
                   "04510169613041179627824850369661627758117344585454952398513078591012"
                   "16177774460245792848745010610272632849144679399483969637136592772675"
                   "30421574188119406015741463062685299233946273066607202191062843450759"
                   "09189691913932415049831533576609434887106277641153470780262690056223"
                   "60536271077885617790509297878092755218865744089197060800946484997385"
                   "01745518501128178558848005710927956044510090930374814326926802000802"
                   "052945569430779028887070667017196503721574400"));
    assert(Integer("0x100000000000000000000000000000000") - Integer("0xffffffffffffffffffffffffffffffff") ==
           one);
    assert(Integer("0xffffffffffffffffffffffffffffffff") + 1 ==
           Integer("0x100000000000000000000000000000000"));
    assert(x1 * 0 == zero && x2 * 0 == zero && x3 * 0 == zero);
    assert(Integer("0x123413451345134513543415135413451345134513451345134513451345123123"
                   "11233567678657895789578995789789deadead") *
               Integer("0xdeadbad12343556375646784976789234820893459023458"
                       "92345800345203459203594203495") ==
           Integer("0xfd5832fa169c01eb16ed0d6c894aa4b6ef35aae11120c8374a1877525101934ab5"
                   "606a9be865f2f93179b51beb369974dba9d9c9230d26b00a8bd1177ef55fe8ee1a34"
                   "db26275af2743a96f97b5e207fde0fa8dd9c330788ebeb1"));
    assert(Integer("0xdeadbeefbadadadeeeaddaccb12341345134513451354341513541345134513451"
                   "345134513451345134512312311233567678657895789578995789789deadead") *
               Integer("0xdeadbad1234355637564678497678923482089345902345892345800345203"
                       "459203594203495adadadadadadeeefefefbadadadedad") ==
           Integer("0xc1b1c97e134a7207be51b9c69e47fd2f9f05532f0c1af67feaa98bfe863dd3c418"
                   "8523f8c24dfef3064dadcf2ac8a63170d09eda4038c6751adbd36172f3df5a04281e"
                   "3df3ce6114c3f4e53bfef0d06db28c138d123a068ac33b10ec6812d97522ef850023"
                   "ec8af45fc87d1c115caaa9591ad18bc7a3e9"));
    assert(Integer("0xdeadbeefbadadadeeeaddaccb12341345134513451354341513541345134513451"
                   "345134513451345134512312311233567678657895789578995789789deadeaddead"
                   "bad12343556375646784976789234820893459023458923458003452034592035942"
                   "03495adadadadadadeeefefefbadadadedad") *
               Integer("0xdeadbad1234355637564678497678923482089345902345892345800345203"
                       "459203594203495adadadadadadeeefefefbadadadedaddeadbeefbadadadeee"
                       "addaccb123413451345134513543415135413451345134513451345134513451"
                       "34512312311233567678657895789578995789789deadead") ==
           Integer("0xc1b1c97e134a7207be51b9c69e47fd2f9f05532f0c1af67feaa98bfe863dd3c418"
                   "8523f8c24dfef3064dadcf2ac8a63170d09eda40398826e7ef39e04c35a0887b0fb6"
                   "ee5f1b7cfa9877f7916ea5007d915593cb2b17e9a4e284b4a819046d6fa483735627"
                   "d220ffa5cb960b05f2ee8521fe2ffa876b4f3e6eb8c549af96de8a97fd982884ec73"
                   "0b9b2775941b46b2b32cfb384219cd8ed2ecf5f8ec547cdffd77d1472a9dbae33ef4"
                   "7716c59fecfef2cf593a3efbd3daed281e3df3ce6114c3f4e53bfef0d06db28c138d"
                   "123a068ac33b10ec6812d97522ef850023ec8af45fc87d1c115caaa9591ad18bc7a3"
                   "e9"));
    assert(Integer("0x89dff44a5cc6cc2749eb05bdacb34a583393e8b33598b804") /
               Integer("0x31eede192bdc2e2699cbd0d634bc8c4d") ==
           Integer("0x2c2dd7dfe2c4cf29e"));
    assert(MPA::power(Integer("-0xdeadbeefdeadbeefdeadbeefdeadbeef"), 13) ==
           Integer("-0x29c70e43c09ddaaae5988e14cbcbe7bb3407e97ae63f1611965d76"
                   "0a2b900025443ef98ad45dd4146e14b39f41c50bf5cc80a58d2c4106cb86bd5093f63ff08a6fdead077b4fcdf1dc0e32858062d91ed7f2c96029aea9a521e1e228cb0b9e97628a7108dcbb1b4741b05a"
                   "bcbe567844c2dbaefc6e3236cd6a517de0e0fe197a12a8b8bf35c10254216deaaad37291d3a9943ac438970eef8dd7007d3f88beb2af61ee59b61348cf7f8855b21d04b01cf4fee7fe71d87a82e4c2aa"
                   "2ab314fcf4edd8b87dfc9145befbf2cefa2d03c42f"));
    assert(Integer("65537") == Integer("0x10001"));

    // string conversions
    assert(x1.to_decimal() == "55515754828527398988712969445402434");
    assert(x2.to_decimal() == "1375590926703372152279585");
    assert(x3.to_decimal() == "222");
    assert(((one) << 128).to_decimal() == "340282366920938463463374607431768211456");
    assert(((one) << 614).to_decimal() == "679856630805461886322672904387159842981879190690600861695288496896516556"
                                          "62189087070612800289949348565617834174239552129964362155219546526644418557282123181048810402666930332036061200384");
    assert(Integer("0xdeabbb12367893424567567555231123123deeaddebeffedda2321").to_decimal() ==
           "91601463601495740755200305805653269924991197392129381132790276897");
    assert(Integer("0xdeabbb12367893424567567555231123123deeaddebeffedda2321deabbb123678"
                   "93424567567555231123123deeaddebeffedda2321")
               .to_decimal() == "964676005206744428849994500826961178146024800492380263298845869474213201"
                                "7591701492587431652951948554776986601395928289084977062689");
    assert(((-one) << 614).to_decimal() == "-679856630805461886322672904387159842981879190690600861695288496896516556"
                                           "62189087070612800289949348565617834174239552129964362155219546526644418557282123181048810402666930332036061200384");
    assert(x1.to_binary() ==
           "0b10101011000100100011010101100111010101100111101011011110111011111111000101000011010101100101011101010110011101000010");
    assert(x2.to_binary() == "0b100100011010010101110111011111101101110111010000100100011001000110001001000100001");
    assert(x3.to_binary() == "0b11011110");
    assert((-x1).to_binary() ==
           "-0b10101011000100100011010101100111010101100111101011011110111011111111000101000011010101100101011101010110011101000010");
    assert((-x2).to_binary() == "-0b100100011010010101110111011111101101110111010000100100011001000110001001000100001");
    assert((-x3).to_binary() == "-0b11011110");
    // modular arithmetic
    {
        Integer base("0x112312334534535241312312313245345345");
        Integer modulus("0x11797897897892312334534535241312312313245345345");
        Integer exponent("0x111123123123123123123123123");
        assert(MPA::modular_power(base, exponent, modulus) ==
               Integer("0x4d3e8ef9f877a4899d1326dd59914a33a1c472033601cc"));
        assert(MPA::modular_power(base, -exponent, modulus) ==
               Integer("0x10c2ffc0cfef84583a46567f0e1f69ef977353ed0d25a44"));
    }
    {
        Integer modulus("0x112312334534535241312312313245345345");
        Integer base("0x11797897897892312334534535241312312313245345345");
        Integer exponent("0x111123123123123123123123123");
        assert(MPA::modular_power(base, exponent, modulus) ==
               Integer("0x1e1845a70ce61e70b2ecad422e0944f9b24"));
        assert(MPA::modular_power(base, -exponent, modulus) ==
               Integer("0x8c6ea49a46682da52c3a8f4ed2c938060bc"));
    }
    {
        Integer base("0x9907cdaa071bdef0");
        Integer exponent("0x7c884f1de8a1645ff7333ba817664339");
        Integer modulus("0x4c3ad5b263c28ef37c36e2c41b688bbf");
        assert(MPA::modular_power(base, exponent, modulus) ==
               Integer("0x2ce6fcbd391c5e1c542155ee932f07a2"));
        assert(MPA::modular_power(base, -exponent, modulus) ==
               Integer("0x5aa3469db7c6bbdd6bce5c177599365"));
    }

    // prime check
    {
        assert(
            MPA::is_probably_prime(Integer("0xea8a03aab3562ee42846b6ae7b3d1504c6f72c4f19c43f20947136c4653d1c0e51825d1f6da69e788d1705c3fd"
                                           "0e5a2373aa09141cd8f48b52e8d6c6bce6394fd0991872386717270c94f0a65cd35649d4c5b06fd0e51748db3b3a"
                                           "fbb29e878320fcf865bac0ffc83e6f08b260aa30a21792e90f1ca92db9129ebb882f2936dda60774e2023fd02ede"
                                           "cc25f456df50c6b060d1003f6b1daef149c0be6643aa414aa3f79af6641aa02fda2cad5dc3f16e44abada2b13140"
                                           "4a37365ab8fad8670ee749df4e9b9045ffe4f5a0ffe9325627b1418345da0c7fa6d3520ecc2a5cef4666753ac273"
                                           "e839772338f932d41afcfcf243391357a1c18917ce067b999a9451")));
        assert(
            !MPA::is_probably_prime(x1 * x2 * x3 * Integer("0xea8a03aab3562ee42846b6ae7b3d1504c6f72c4f19c43f20947136c4653d1c0e51825d1f6da69e788d1705c3fd"
                                                           "0e5a2373aa09141cd8f48b52e8d6c6bce6394fd0991872386717270c94f0a65cd35649d4c5b06fd0e51748db3b3a"
                                                           "fbb29e878320fcf865bac0ffc83e6f08b260aa30a21792e90f1ca92db9129ebb882f2936dda60774e2023fd02ede"
                                                           "cc25f456df50c6b060d1003f6b1daef149c0be6643aa414aa3f79af6641aa02fda2cad5dc3f16e44abada2b13140"
                                                           "4a37365ab8fad8670ee749df4e9b9045ffe4f5a0ffe9325627b1418345da0c7fa6d3520ecc2a5cef4666753ac273"
                                                           "e839772338f932d41afcfcf243391357a1c18917ce067b999a9451")));
        assert(
            MPA::is_probably_prime(Integer("0xbbe0024fadda17210f282a4575bd9ad06c50e50d0f2ad2b8b8667338706ae6bec9a0a9e4b9af2a8f5d22727b14"
                                           "7fe97675ba73f095b9f2c472d490c5109b108a3070893b995c8f2c13fa3f0caabdcc366114cd8b75793c9a5160d8"
                                           "d3d8fa0873cc7ae18786cf2796143506b2eda0b35c5202871d263ec4ae36d9fe7686467cdef3caeecdaeb30fa22a"
                                           "f93e7eb51aad1a4bac42daaec5f9222ab9e311118cfdd7c0da30dbea435fe3dd93e3af5b5fa8188d1db58ac50706"
                                           "9a75a6c48888f0ab465a0692a4554af51efe7456e1eaadab023ce82cf06cfa231f55c1e67d3b86dfde03658fc595"
                                           "8717ef97ff32e21c256ab26480ff03565db3bb1866612e50d96119")));
        assert(
            !MPA::is_probably_prime(x1 * x2 * x3 * Integer("0xbbe0024fadda17210f282a4575bd9ad06c50e50d0f2ad2b8b8667338706ae6bec9a0a9e4b9af2a8f5d22727b14"
                                                           "7fe97675ba73f095b9f2c472d490c5109b108a3070893b995c8f2c13fa3f0caabdcc366114cd8b75793c9a5160d8"
                                                           "d3d8fa0873cc7ae18786cf2796143506b2eda0b35c5202871d263ec4ae36d9fe7686467cdef3caeecdaeb30fa22a"
                                                           "f93e7eb51aad1a4bac42daaec5f9222ab9e311118cfdd7c0da30dbea435fe3dd93e3af5b5fa8188d1db58ac50706"
                                                           "9a75a6c48888f0ab465a0692a4554af51efe7456e1eaadab023ce82cf06cfa231f55c1e67d3b86dfde03658fc595"
                                                           "8717ef97ff32e21c256ab26480ff03565db3bb1866612e50d96119")));
        assert(
            MPA::is_probably_prime(Integer("0x9ec6a721e70e697a8d38837420ee1c24fdcb5ac8c5df73808e1c6ac551dc3b9c898bdf78c4a41bb40bdcf965c4"
                                           "9741152534d820b0603dc5446dfff7d174bad84f1810e634ff36548c2a74cdb8bc8fcde8a1a054a16f208d16f3b6"
                                           "01109b441790deb083af9461afecbf921468effe6049362ed3dfa32ccf9580e22a699d24d45df0a47f8230dc54e2"
                                           "af53ec94d762f2dd3585729d68447650710ac0619d9e190eb6cf4fb80e484bbc57210a66517d9dbbd162ad73710e"
                                           "13e3bc349ecfc1aa33b132447cf7ff7c3a2707a793f15e24da224dc8c50fe70f282073324448f10a57e976048067"
                                           "ef51b1152fc5bd431d0da1735bd0897d46dcb341e15ed4e6e614d7")));
        assert(
            !MPA::is_probably_prime(x1 * x2 * x3 * Integer("0x9ec6a721e70e697a8d38837420ee1c24fdcb5ac8c5df73808e1c6ac551dc3b9c898bdf78c4a41bb40bdcf965c4"
                                                           "9741152534d820b0603dc5446dfff7d174bad84f1810e634ff36548c2a74cdb8bc8fcde8a1a054a16f208d16f3b6"
                                                           "01109b441790deb083af9461afecbf921468effe6049362ed3dfa32ccf9580e22a699d24d45df0a47f8230dc54e2"
                                                           "af53ec94d762f2dd3585729d68447650710ac0619d9e190eb6cf4fb80e484bbc57210a66517d9dbbd162ad73710e"
                                                           "13e3bc349ecfc1aa33b132447cf7ff7c3a2707a793f15e24da224dc8c50fe70f282073324448f10a57e976048067"
                                                           "ef51b1152fc5bd431d0da1735bd0897d46dcb341e15ed4e6e614d7")));
        assert(
            MPA::is_probably_prime(Integer("0x9dd5733002417def33bf9c62f2c348446e8782d39c7d9caf7f194d6ae3efc5c6dbc7f853905d1acd16084f0529"
                                           "684aefc260ef416ed55e3323d7fbc30896a7d4e610feec156bc0afab04f12643fc4c668084cb7aea275530bff227"
                                           "51edcbe1c750f4aae55f22ec68f6c2d075e112dbf998610665031d59fa108e32999ef02ec6fd70ad6b58c9ed07dd"
                                           "172c4b489d9c314341b197e71bbf46eb1695ec03805dac9737ee2651b1f5c373aca8626b7dfac871855e41b9af3f"
                                           "fae6b33fc3ab36a041ea2a8f2b293ad7e69707569e23927ad35c5385d921f14f55d25e2fc38a988572e454ea679c"
                                           "ba630854f58f3ad75f3753ac2d959cb4260a429667566209b088c1")));
        assert(
            !MPA::is_probably_prime(x1 * x2 * x3 * Integer("0x9dd5733002417def33bf9c62f2c348446e8782d39c7d9caf7f194d6ae3efc5c6dbc7f853905d1acd16084f0529"
                                                           "684aefc260ef416ed55e3323d7fbc30896a7d4e610feec156bc0afab04f12643fc4c668084cb7aea275530bff227"
                                                           "51edcbe1c750f4aae55f22ec68f6c2d075e112dbf998610665031d59fa108e32999ef02ec6fd70ad6b58c9ed07dd"
                                                           "172c4b489d9c314341b197e71bbf46eb1695ec03805dac9737ee2651b1f5c373aca8626b7dfac871855e41b9af3f"
                                                           "fae6b33fc3ab36a041ea2a8f2b293ad7e69707569e23927ad35c5385d921f14f55d25e2fc38a988572e454ea679c"
                                                           "ba630854f58f3ad75f3753ac2d959cb4260a429667566209b088c1")));
        assert(
            MPA::is_probably_prime(Integer("0xaa10034907a5e9cb1987c1b92026fbba4c603bf4e3f79ab300d2fc6967e2530edb38681531b9b02f9a1c0e551f"
                                           "b11ec67f70d953288ff23c274e14d26e4bc268e684b4986c0ce56cb20c46f95c8e9b7a99ec74509593a4bf0b871d"
                                           "d126f0f20356d1f8d2bb16a97b15a569efa88553f0e057e814a1e4c423486b7b57b99c27a39749b1d19aef834c6f"
                                           "0d950dfc4cabf658f8fd12aca643237ae32b3a381a64ad6aea2fd6d5bd9d1d3e54e370cc02bd5c290f0539d5331c"
                                           "4167c31f02187ed67a2cc377d09d19876a020d50cd3251b9037ae76946d9a8b1276ebdb4d4b673bd385678b66f5a"
                                           "ef459122523aaf92dd9f2b7a517b04b90b2c99f27ebcbb94996d57")));
        assert(
            !MPA::is_probably_prime(x1 * x2 * x3 * Integer("0xaa10034907a5e9cb1987c1b92026fbba4c603bf4e3f79ab300d2fc6967e2530edb38681531b9b02f9a1c0e551f"
                                                           "b11ec67f70d953288ff23c274e14d26e4bc268e684b4986c0ce56cb20c46f95c8e9b7a99ec74509593a4bf0b871d"
                                                           "d126f0f20356d1f8d2bb16a97b15a569efa88553f0e057e814a1e4c423486b7b57b99c27a39749b1d19aef834c6f"
                                                           "0d950dfc4cabf658f8fd12aca643237ae32b3a381a64ad6aea2fd6d5bd9d1d3e54e370cc02bd5c290f0539d5331c"
                                                           "4167c31f02187ed67a2cc377d09d19876a020d50cd3251b9037ae76946d9a8b1276ebdb4d4b673bd385678b66f5a"
                                                           "ef459122523aaf92dd9f2b7a517b04b90b2c99f27ebcbb94996d57")));
        assert(
            MPA::is_probably_prime(Integer("0xb3320e2fc516f32158b510e30530540cafd8f0a293aa20511d938b2a1faaca425ff9ff63f4ff4ae05d4c499335"
                                           "c2951505c6d96e2c53506229b5244a884c83e7")));
        assert(
            !MPA::is_probably_prime(x1 * x2 * x3 * Integer("0xb3320e2fc516f32158b510e30530540cafd8f0a293aa20511d938b2a1faaca425ff9ff63f4ff4ae05d4c499335"
                                                           "c2951505c6d96e2c53506229b5244a884c83e7")));
        assert(
            MPA::is_probably_prime(Integer("0xebd8efaf6ca7f48d4c3b1993c87222ad4fd3cf954d1c2e44bf129e17fb0685c6c8800a585ec5017aa4e53ec37a"
                                           "33e5b9dad31c4f1ba90e790ca93fe9c21284bf")));
        assert(
            !MPA::is_probably_prime(x1 * x2 * x3 * Integer("0xebd8efaf6ca7f48d4c3b1993c87222ad4fd3cf954d1c2e44bf129e17fb0685c6c8800a585ec5017aa4e53ec37a"
                                                           "33e5b9dad31c4f1ba90e790ca93fe9c21284bf")));
        assert(
            MPA::is_probably_prime(Integer("0xc30bbf9dbeefcf9ea2ef133cd41a8f11280de895afb34563bbcbdf854c204b60c9e30441db46c15c19cff57aba"
                                           "05cdb3059691296f671423c180c47ef9990c81")));
        assert(
            !MPA::is_probably_prime(x1 * x2 * x3 * Integer("0xc30bbf9dbeefcf9ea2ef133cd41a8f11280de895afb34563bbcbdf854c204b60c9e30441db46c15c19cff57aba"
                                                           "05cdb3059691296f671423c180c47ef9990c81")));
        assert(
            MPA::is_probably_prime(Integer("0xfe172523d34f74b42b0cbd9bfd025ee11796ce71c788bc0a3ff2908871750fac21714e780cac8873bd45c086b9"
                                           "34c3d34d2579319ed4bc37b54a6dfa03fe5813")));
        assert(
            !MPA::is_probably_prime(x1 * x2 * x3 * Integer("0xfe172523d34f74b42b0cbd9bfd025ee11796ce71c788bc0a3ff2908871750fac21714e780cac8873bd45c086b9"
                                                           "34c3d34d2579319ed4bc37b54a6dfa03fe5813")));
        assert(
            MPA::is_probably_prime(Integer("0xf6973f67c8addd54f4666f33460a7553143298867271b05f8ef941d62a7727fd214f0c93a6215ff374c32b2b9f"
                                           "b3b529765771fb6242da8522a2b06620699540420cdcb6465192a017696440a46e69ac5ba0c254cef53980166941"
                                           "ea15d483c6edc7158067d6f6dad665a7ae12a6998de026a94b76b5271f6e9a0a7736b9e7f7")));
        assert(
            !MPA::is_probably_prime(x1 * x2 * x3 * Integer("0xf6973f67c8addd54f4666f33460a7553143298867271b05f8ef941d62a7727fd214f0c93a6215ff374c32b2b9f"
                                                           "b3b529765771fb6242da8522a2b06620699540420cdcb6465192a017696440a46e69ac5ba0c254cef53980166941"
                                                           "ea15d483c6edc7158067d6f6dad665a7ae12a6998de026a94b76b5271f6e9a0a7736b9e7f7")));
        assert(
            MPA::is_probably_prime(Integer("0xf8046e1916fe6771c22ab910f970204b5bb2e6190e28a44b8bd6b7b3dda48f746b1529a46b44a3096ffd6caca8"
                                           "735e433b0703b3c069802a62b06a8a6084e1cd3e48ffa024a3df0c954617cc9fda15b0736188792230a9939c7669"
                                           "5826fd67f2f2d4d4db07b88cef012f272c465025764697fdbccc31e166322ea9615554d639")));
        assert(
            !MPA::is_probably_prime(x1 * x2 * x3 * Integer("0xf8046e1916fe6771c22ab910f970204b5bb2e6190e28a44b8bd6b7b3dda48f746b1529a46b44a3096ffd6caca8"
                                                           "735e433b0703b3c069802a62b06a8a6084e1cd3e48ffa024a3df0c954617cc9fda15b0736188792230a9939c7669"
                                                           "5826fd67f2f2d4d4db07b88cef012f272c465025764697fdbccc31e166322ea9615554d639")));
        assert(
            MPA::is_probably_prime(Integer("0xce4974722d1c0a416a76406ada4ca93015c1b3de6a71d5dd5d075f507172c8ab93ef560156c302dc8e38775ba7"
                                           "f15b953fa484182557fa9caae8ae9b36f63779c8417c7f30f536c7ba66d09ac0afb8163797f4a769f63b38829664"
                                           "62b71be804517a98fbe8f575ab04443b08519364c52533c9069774830047ec2996ec0f3425")));
        assert(
            !MPA::is_probably_prime(x1 * x2 * x3 * Integer("0xce4974722d1c0a416a76406ada4ca93015c1b3de6a71d5dd5d075f507172c8ab93ef560156c302dc8e38775ba7"
                                                           "f15b953fa484182557fa9caae8ae9b36f63779c8417c7f30f536c7ba66d09ac0afb8163797f4a769f63b38829664"
                                                           "62b71be804517a98fbe8f575ab04443b08519364c52533c9069774830047ec2996ec0f3425")));
        assert(
            MPA::is_probably_prime(Integer("0xa723b3ce2c9944e714e6693b1e4b38797671d0ebce7665b0b0cd0368105c793b70e016cda4ba47b6c100a2a38f"
                                           "7858f33b29e77ee4995447a3288ce41078ec6d73d03b743a89ed8fb4c897ad0c143a074653d1aebba54079499b97"
                                           "c9c7d2b630603ad5d42e2efa91d10727a4958ee3b7896f156209b05ea6fe24aba4fbb7e15f")));
        assert(
            !MPA::is_probably_prime(x1 * x2 * x3 * Integer("0xa723b3ce2c9944e714e6693b1e4b38797671d0ebce7665b0b0cd0368105c793b70e016cda4ba47b6c100a2a38f"
                                                           "7858f33b29e77ee4995447a3288ce41078ec6d73d03b743a89ed8fb4c897ad0c143a074653d1aebba54079499b97"
                                                           "c9c7d2b630603ad5d42e2efa91d10727a4958ee3b7896f156209b05ea6fe24aba4fbb7e15f")));
        assert(!MPA::is_probably_prime((Integer(
                                            "0xea8a03aab3562ee42846b6ae7b3d1504c6f72c4f19c43f20947136c4653d1c0e51825d1f6da69e788d1705c3fd"
                                            "0e5a2373aa09141cd8f48b52e8d6c6bce6394fd0991872386717270c94f0a65cd35649d4c5b06fd0e51748db3b3a"
                                            "fbb29e878320fcf865bac0ffc83e6f08b260aa30a21792e90f1ca92db9129ebb882f2936dda60774e2023fd02ede"
                                            "cc25f456df50c6b060d1003f6b1daef149c0be6643aa414aa3f79af6641aa02fda2cad5dc3f16e44abada2b13140"
                                            "4a37365ab8fad8670ee749df4e9b9045ffe4f5a0ffe9325627b1418345da0c7fa6d3520ecc2a5cef4666753ac273"
                                            "e839772338f932d41afcfcf243391357a1c18917ce067b999a9451") -
                                        1)));
        assert(MPA::is_probably_prime(MPA::get_random_prime<word_t>(8)));
        assert(MPA::is_probably_prime(MPA::get_random_prime<word_t>(16)));
        assert(MPA::is_probably_prime(MPA::get_random_prime<word_t>(32)));
        assert(!MPA::is_probably_prime(MPA::get_random_prime<word_t>(8) * MPA::get_random_prime<word_t>(8)));
        assert(!MPA::is_probably_prime(MPA::get_random_prime<word_t>(16) * MPA::get_random_prime<word_t>(16)));
        assert(!MPA::is_probably_prime(MPA::get_random_prime<word_t>(32) * MPA::get_random_prime<word_t>(32)));
    }

    // bitwise operators
    {
        Integer x("0xabdeaf1234355512313123a");
        Integer y("0xabdeadeeeeeeeeeeeeeee");
        Integer z("0xfffffffffffffffffffff");
        Integer w("0xabdeaf1aaa35551231312");
        assert((x | y) == Integer("0xabffffbffefffffeffffefe"));
        assert((x | z) == Integer("0xabfffffffffffffffffffff"));
        assert((x | w) == Integer("0xabffffbf3ebf7557333133a"));
        assert((y | z) == Integer("0xfffffffffffffffffffff"));
        assert((y | w) == Integer("0xabdeaffeeefffffeffffe"));
        assert((z | w) == Integer("0xfffffffffffffffffffff"));
        assert((x & y) == Integer("0x8a8e0024244402202022a"));
        assert((x & z) == Integer("0xdeaf1234355512313123a"));
        assert((x & w) == Integer("0x8a8e02102015101031212"));
        assert((y & z) == Integer("0xabdeadeeeeeeeeeeeeeee"));
        assert((y & w) == Integer("0xabdead0aaa24440220202"));
        assert((z & w) == Integer("0xabdeaf1aaa35551231312"));
        assert((x ^ y) == Integer("0xab7571bfdadbbbfcdfdfcd4"));
        assert((x ^ z) == Integer("0xab2150edcbcaaaedcecedc5"));
        assert((x ^ w) == Integer("0xab7571bd2e9f60472300128"));
        assert((y ^ z) == Integer("0x542152111111111111111"));
        assert((y ^ w) == Integer("0x2f444dbbbfcdfdfc"));
        assert((z ^ w) == Integer("0x542150e555caaaedceced"));
    }
}

int main()
{
    std::cout << "running tests ...\n";
    test_integer<uint16_t>();
    std::cout << "wordtype uint16_t OK\n";
    test_integer<uint32_t>();
    std::cout << "wordtype uint32_t OK\n";
#ifdef __SIZEOF_INT128__
    test_integer<uint64_t>();
    std::cout << "wordtype uint64_t OK\n";
#endif
    std::cout << "all tests passed\n";
    return 0;
}

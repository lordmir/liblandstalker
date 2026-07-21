#include <landstalker/text/Charset.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <landstalker/misc/Utils.h>
#include <landstalker/text/IntroString.h>
#include <landstalker/text/EndCreditString.h>

namespace Landstalker {
namespace Charset {

const LSString::CharacterSet DEFAULT_ENGLISH_CHARSET =
{
    { 0_u8, L" "}, { 1_u8, L"0"}, { 2_u8, L"1"}, { 3_u8, L"2"}, { 4_u8, L"3"}, { 5_u8, L"4"}, { 6_u8, L"5"}, { 7_u8, L"6"},
    { 8_u8, L"7"}, { 9_u8, L"8"}, {10_u8, L"9"}, {11_u8, L"A"}, {12_u8, L"B"}, {13_u8, L"C"}, {14_u8, L"D"}, {15_u8, L"E"},
    {16_u8, L"F"}, {17_u8, L"G"}, {18_u8, L"H"}, {19_u8, L"I"}, {20_u8, L"J"}, {21_u8, L"K"}, {22_u8, L"L"}, {23_u8, L"M"},
    {24_u8, L"N"}, {25_u8, L"O"}, {26_u8, L"P"}, {27_u8, L"Q"}, {28_u8, L"R"}, {29_u8, L"S"}, {30_u8, L"T"}, {31_u8, L"U"},
    {32_u8, L"V"}, {33_u8, L"W"}, {34_u8, L"X"}, {35_u8, L"Y"}, {36_u8, L"Z"}, {37_u8, L"a"}, {38_u8, L"b"}, {39_u8, L"c"},
    {40_u8, L"d"}, {41_u8, L"e"}, {42_u8, L"f"}, {43_u8, L"g"}, {44_u8, L"h"}, {45_u8, L"i"}, {46_u8, L"j"}, {47_u8, L"k"},
    {48_u8, L"l"}, {49_u8, L"m"}, {50_u8, L"n"}, {51_u8, L"o"}, {52_u8, L"p"}, {53_u8, L"q"}, {54_u8, L"r"}, {55_u8, L"s"},
    {56_u8, L"t"}, {57_u8, L"u"}, {58_u8, L"v"}, {59_u8, L"w"}, {60_u8, L"x"}, {61_u8, L"y"}, {62_u8, L"z"}, {63_u8, L"*"},
    {64_u8, L"."}, {65_u8, L","}, {66_u8, L"?"}, {67_u8, L"!"}, {68_u8, L"/"}, {69_u8, L"<"}, {70_u8, L">"}, {71_u8, L":"},
    {72_u8, L"-"}, {73_u8, L"\'"}, {74_u8, L"\""}, {75_u8, L"%"}, {76_u8, L"#"}, {77_u8, L"&"}, {78_u8, L"("}, {79_u8, L")"},
    {80_u8, L"="}, {81_u8, L"↖"}, {82_u8, L"↗"}, {83_u8, L"↘"}, {84_u8, L"↙"}
};

const LSString::CharacterSet DEFAULT_FRENCH_CHARSET = {
    { 0_u8, L" "}, { 1_u8, L"0"}, { 2_u8, L"1"}, { 3_u8, L"2"}, { 4_u8, L"3"}, { 5_u8, L"4"}, { 6_u8, L"5"}, { 7_u8, L"6"},
    { 8_u8, L"7"}, { 9_u8, L"8"}, {10_u8, L"9"}, {11_u8, L"A"}, {12_u8, L"B"}, {13_u8, L"C"}, {14_u8, L"D"}, {15_u8, L"E"},
    {16_u8, L"F"}, {17_u8, L"G"}, {18_u8, L"H"}, {19_u8, L"I"}, {20_u8, L"J"}, {21_u8, L"K"}, {22_u8, L"L"}, {23_u8, L"M"},
    {24_u8, L"N"}, {25_u8, L"O"}, {26_u8, L"P"}, {27_u8, L"Q"}, {28_u8, L"R"}, {29_u8, L"S"}, {30_u8, L"T"}, {31_u8, L"U"},
    {32_u8, L"V"}, {33_u8, L"W"}, {34_u8, L"X"}, {35_u8, L"Y"}, {36_u8, L"Z"}, {37_u8, L"a"}, {38_u8, L"b"}, {39_u8, L"c"},
    {40_u8, L"d"}, {41_u8, L"e"}, {42_u8, L"f"}, {43_u8, L"g"}, {44_u8, L"h"}, {45_u8, L"i"}, {46_u8, L"j"}, {47_u8, L"k"},
    {48_u8, L"l"}, {49_u8, L"m"}, {50_u8, L"n"}, {51_u8, L"o"}, {52_u8, L"p"}, {53_u8, L"q"}, {54_u8, L"r"}, {55_u8, L"s"},
    {56_u8, L"t"}, {57_u8, L"u"}, {58_u8, L"v"}, {59_u8, L"w"}, {60_u8, L"x"}, {61_u8, L"y"}, {62_u8, L"z"}, {63_u8, L"*"},
    {64_u8, L"."}, {65_u8, L","}, {66_u8, L"?"}, {67_u8, L"!"}, {68_u8, L"/"}, {69_u8, L"<"}, {70_u8, L">"}, {71_u8, L":"},
    {72_u8, L"-"}, {73_u8, L"\'"}, {74_u8, L"\""}, {75_u8, L"%"}, {76_u8, L"#"}, {77_u8, L"&"}, {78_u8, L"("}, {79_u8, L")"},
    {80_u8, L"="}, {81_u8, L"↖"}, {82_u8, L"↗"}, {83_u8, L"↘"}, {84_u8, L"↙"}, {85_u8, L"é"}, {86_u8, L"à"}, {87_u8, L"è"},
    {88_u8, L"ù"}, {89_u8, L"â"}, {90_u8, L"ê"}, {91_u8, L"î"}, {92_u8, L"ô"}, {93_u8, L"û"}, {94_u8, L"ç"}, {95_u8, L"ë"},
    {96_u8, L"ï"}, {97_u8, L"ü"}, {98_u8, L";"}, {99_u8, L"`"}
};

const LSString::CharacterSet DEFAULT_GERMAN_CHARSET = {
    { 0_u8, L" "}, { 1_u8, L"0"}, { 2_u8, L"1"}, { 3_u8, L"2"}, { 4_u8, L"3"}, { 5_u8, L"4"}, { 6_u8, L"5"}, { 7_u8, L"6"},
    { 8_u8, L"7"}, { 9_u8, L"8"}, {10_u8, L"9"}, {11_u8, L"A"}, {12_u8, L"B"}, {13_u8, L"C"}, {14_u8, L"D"}, {15_u8, L"E"},
    {16_u8, L"F"}, {17_u8, L"G"}, {18_u8, L"H"}, {19_u8, L"I"}, {20_u8, L"J"}, {21_u8, L"K"}, {22_u8, L"L"}, {23_u8, L"M"},
    {24_u8, L"N"}, {25_u8, L"O"}, {26_u8, L"P"}, {27_u8, L"Q"}, {28_u8, L"R"}, {29_u8, L"S"}, {30_u8, L"T"}, {31_u8, L"U"},
    {32_u8, L"V"}, {33_u8, L"W"}, {34_u8, L"X"}, {35_u8, L"Y"}, {36_u8, L"Z"}, {37_u8, L"*"}, {38_u8, L"."}, {39_u8, L","},
    {40_u8, L"?"}, {41_u8, L"!"}, {42_u8, L"/"}, {43_u8, L"<"}, {44_u8, L">"}, {45_u8, L":"}, {46_u8, L"-"}, {47_u8, L"\'"},
    {48_u8, L"\""}, {49_u8, L"%"}, {50_u8, L"#"}, {51_u8, L"&"}, {52_u8, L"("}, {53_u8, L")"}, {54_u8, L"="}, {55_u8, L"↖"},
    {56_u8, L"↗"}, {57_u8, L"↘"}, {58_u8, L"↙"}, {59_u8, L"Ä"}, {60_u8, L"Ö"}, {61_u8, L"Ü"}, {62_u8, L"ß"},
    {63_u8, L";"}, {64_u8, L"`"}
};

const LSString::CharacterSet DEFAULT_JAPANESE_CHARSET =
{
    {  0_u8, L" "}, {  1_u8, L"0"}, {  2_u8, L"1"}, {  3_u8, L"2"}, {  4_u8, L"3"}, {  5_u8, L"4"}, {  6_u8, L"5"}, {  7_u8, L"6"},
    {  8_u8, L"7"}, {  9_u8, L"8"}, { 10_u8, L"9"}, { 11_u8, L"あ"}, { 12_u8, L"い"}, { 13_u8, L"う"}, { 14_u8, L"え"}, { 15_u8, L"お"},
    { 16_u8, L"か"}, { 17_u8, L"き"}, { 18_u8, L"く"}, { 19_u8, L"け"}, { 20_u8, L"こ"}, { 21_u8, L"さ"}, { 22_u8, L"し"}, { 23_u8, L"す"},
    { 24_u8, L"せ"}, { 25_u8, L"そ"}, { 26_u8, L"た"}, { 27_u8, L"ち"}, { 28_u8, L"つ"}, { 29_u8, L"て"}, { 30_u8, L"と"}, { 31_u8, L"な"},
    { 32_u8, L"に"}, { 33_u8, L"ぬ"}, { 34_u8, L"ね"}, { 35_u8, L"の"}, { 36_u8, L"は"}, { 37_u8, L"ひ"}, { 38_u8, L"ふ"}, { 39_u8, L"へ"},
    { 40_u8, L"ほ"}, { 41_u8, L"ま"}, { 42_u8, L"み"}, { 43_u8, L"む"}, { 44_u8, L"め"}, { 45_u8, L"も"}, { 46_u8, L"や"}, { 47_u8, L"ゆ"},
    { 48_u8, L"よ"}, { 49_u8, L"ら"}, { 50_u8, L"り"}, { 51_u8, L"る"}, { 52_u8, L"れ"}, { 53_u8, L"ろ"}, { 54_u8, L"わ"}, { 55_u8, L"を"},
    { 56_u8, L"ん"}, { 57_u8, L"ぁ"}, { 58_u8, L"ぃ"}, { 59_u8, L"ぅ"}, { 60_u8, L"ぇ"}, { 61_u8, L"ぉ"}, { 62_u8, L"ゃ"}, { 63_u8, L"ゅ"},
    { 64_u8, L"ょ"}, { 65_u8, L"っ"}, { 66_u8, L"ア"}, { 67_u8, L"イ"}, { 68_u8, L"ウ"}, { 69_u8, L"エ"}, { 70_u8, L"オ"}, { 71_u8, L"カ"},
    { 72_u8, L"キ"}, { 73_u8, L"ク"}, { 74_u8, L"ケ"}, { 75_u8, L"コ"}, { 76_u8, L"サ"}, { 77_u8, L"シ"}, { 78_u8, L"ス"}, { 79_u8, L"セ"},
    { 80_u8, L"ソ"}, { 81_u8, L"タ"}, { 82_u8, L"チ"}, { 83_u8, L"ツ"}, { 84_u8, L"テ"}, { 85_u8, L"ト"}, { 86_u8, L"ナ"}, { 87_u8, L"ニ"},
    { 88_u8, L"ヌ"}, { 89_u8, L"ネ"}, { 90_u8, L"ノ"}, { 91_u8, L"ハ"}, { 92_u8, L"ヒ"}, { 93_u8, L"フ"}, { 94_u8, L"ヘ"}, { 95_u8, L"ホ"},
    { 96_u8, L"マ"}, { 97_u8, L"ミ"}, { 98_u8, L"ム"}, { 99_u8, L"メ"}, {100_u8, L"モ"}, {101_u8, L"ヤ"}, {102_u8, L"ユ"}, {103_u8, L"ヨ"},
    {104_u8, L"ラ"}, {105_u8, L"リ"}, {106_u8, L"ル"}, {107_u8, L"レ"}, {108_u8, L"ロ"}, {109_u8, L"ワ"}, {110_u8, L"ヲ"}, {111_u8, L"ン"},
    {112_u8, L"ァ"}, {113_u8, L"ィ"}, {114_u8, L"ゥ"}, {115_u8, L"ェ"}, {116_u8, L"ォ"}, {117_u8, L"ャ"}, {118_u8, L"ュ"}, {119_u8, L"ョ"},
    {120_u8, L"ッ"}, {121_u8, L"、"}, {122_u8, L"。"}, {123_u8, L"゛"}, {124_u8, L"゜"}, {125_u8, L"ー"}, {126_u8, L"!"}, {127_u8, L"?"},
    {128_u8, L"※"}, {129_u8, L"東"}, {130_u8, L"西"}, {131_u8, L"南"}, {132_u8, L"北"}, {133_u8, L"上"}, {134_u8, L"中"}, {135_u8, L"下"},
    {136_u8, L"道"}, {137_u8, L"具"}, {138_u8, L"屋"}, {139_u8, L"教"}, {140_u8, L"会"}, {141_u8, L"宿"}, {142_u8, L"神"}, {143_u8, L"父"},
    {144_u8, L"冒"}, {145_u8, L"険"}, {146_u8, L"記"}, {147_u8, L"録"}, {148_u8, L"毒"}, {149_u8, L"呪"}, {150_u8, L"治"}, {151_u8, L"療"},
    {152_u8, L"金"}, {153_u8, L"貨"}, {154_u8, L"枚"}, {155_u8, L"買"}, {156_u8, L"階"}, {157_u8, L"本"}, {158_u8, L"売"}, {159_u8, L"泊"},
    {160_u8, L"客"}, {161_u8, L"品"}, {162_u8, L"男"}, {163_u8, L"女"}, {164_u8, L"子"}, {165_u8, L"供"}, {166_u8, L"人"}, {167_u8, L"族"},
    {168_u8, L"殿"}, {169_u8, L"公"}, {170_u8, L"爵"}, {171_u8, L"領"}, {172_u8, L"主"}, {173_u8, L"兵"}, {174_u8, L"悪"}, {175_u8, L"霊"},
    {176_u8, L"年"}, {177_u8, L"月"}, {178_u8, L"日"}, {179_u8, L"財"}, {180_u8, L"宝"}, {181_u8, L"地"}, {182_u8, L"図"}, {183_u8, L"実"},
    {184_u8, L"灯"}, {185_u8, L"台"}, {186_u8, L"家"}, {187_u8, L"店"}, {188_u8, L"町"}, {189_u8, L"村"}, {190_u8, L"滝"}, {191_u8, L"岬"},
    {192_u8, L"島"}, {193_u8, L"海"}, {194_u8, L"沼"}, {195_u8, L"湖"}, {196_u8, L"港"}, {197_u8, L"城"}, {198_u8, L"塔"}, {199_u8, L"森"},
    {200_u8, L"橋"}, {201_u8, L"団"}, {202_u8, L"気"}, {203_u8, L"船"}, {204_u8, L"箱"}, {205_u8, L"魔"}, {206_u8, L"命"}, {207_u8, L"危"},
    {208_u8, L"美"}, {209_u8, L"長"}, {210_u8, L"古"}, {211_u8, L"老"}, {212_u8, L"作"}, {213_u8, L"名"}, {214_u8, L"商"}, {215_u8, L"大"},
    {216_u8, L"・"}, {217_u8, L"「"}, {218_u8, L"」"}, {219_u8, L"↘"}, {220_u8, L"↖"}, {221_u8, L"↙"}, {222_u8, L"↗"}, {223_u8, L"王"},
    {224_u8, L"剣"}, {225_u8, L"士"}, {226_u8, L"国"}, {227_u8, L"本"}, {228_u8, L"法"}, {229_u8, L"A"}, {230_u8, L"B"}, {231_u8, L"C"},
    {232_u8, L"."}, {236_u8, L"╳"}															   
};

const LSString::CharacterSet MENU_ENGLISH_CHARSET = {
    { 0_u8, L" "}, { 1_u8, L"0"}, { 2_u8, L"1"}, { 3_u8, L"2"}, { 4_u8, L"3"}, { 5_u8, L"4"}, { 6_u8, L"5"}, { 7_u8, L"6"},
    { 8_u8, L"7"}, { 9_u8, L"8"}, {10_u8, L"9"}, {11_u8, L"A"}, {12_u8, L"B"}, {13_u8, L"C"}, {14_u8, L"D"}, {15_u8, L"E"},
    {16_u8, L"F"}, {17_u8, L"G"}, {18_u8, L"H"}, {19_u8, L"I"}, {20_u8, L"J"}, {21_u8, L"K"}, {22_u8, L"L"}, {23_u8, L"M"},
    {24_u8, L"N"}, {25_u8, L"O"}, {26_u8, L"P"}, {27_u8, L"Q"}, {28_u8, L"R"}, {29_u8, L"S"}, {30_u8, L"T"}, {31_u8, L"U"},
    {32_u8, L"V"}, {33_u8, L"W"}, {34_u8, L"X"}, {35_u8, L"Y"}, {36_u8, L"Z"}, {37_u8, L"a"}, {38_u8, L"b"}, {39_u8, L"c"},
    {40_u8, L"d"}, {41_u8, L"e"}, {42_u8, L"f"}, {43_u8, L"g"}, {44_u8, L"h"}, {45_u8, L"i"}, {46_u8, L"j"}, {47_u8, L"k"},
    {48_u8, L"l"}, {49_u8, L"m"}, {50_u8, L"n"}, {51_u8, L"o"}, {52_u8, L"p"}, {53_u8, L"q"}, {54_u8, L"r"}, {55_u8, L"s"},
    {56_u8, L"t"}, {57_u8, L"u"}, {58_u8, L"v"}, {59_u8, L"w"}, {60_u8, L"x"}, {61_u8, L"y"}, {62_u8, L"z"}, {63_u8, L"*"},
    {64_u8, L"."}, {65_u8, L","}, {66_u8, L"?"}, {67_u8, L"!"}, {68_u8, L"/"}, {69_u8, L"<"}, {70_u8, L">"}, {71_u8, L":"},
    {72_u8, L"-"}, {73_u8, L"\'"}, {74_u8, L"\""}, {75_u8, L"%"}, {76_u8, L"#"}, {77_u8, L"&"}, {78_u8, L"("}, {79_u8, L")"},
    {80_u8, L"="}, {81_u8, L"↖"}, {82_u8, L"↗"}, {83_u8, L"↘"}, {84_u8, L"↙"}, {85_u8, L"×"}
};

const LSString::CharacterSet MENU_FRENCH_CHARSET = {
    { 0_u8, L" "}, { 1_u8, L"0"}, { 2_u8, L"1"}, { 3_u8, L"2"}, { 4_u8, L"3"}, { 5_u8, L"4"}, { 6_u8, L"5"}, { 7_u8, L"6"},
    { 8_u8, L"7"}, { 9_u8, L"8"}, {10_u8, L"9"}, {11_u8, L"A"}, {12_u8, L"B"}, {13_u8, L"C"}, {14_u8, L"D"}, {15_u8, L"E"},
    {16_u8, L"F"}, {17_u8, L"G"}, {18_u8, L"H"}, {19_u8, L"I"}, {20_u8, L"J"}, {21_u8, L"K"}, {22_u8, L"L"}, {23_u8, L"M"},
    {24_u8, L"N"}, {25_u8, L"O"}, {26_u8, L"P"}, {27_u8, L"Q"}, {28_u8, L"R"}, {29_u8, L"S"}, {30_u8, L"T"}, {31_u8, L"U"},
    {32_u8, L"V"}, {33_u8, L"W"}, {34_u8, L"X"}, {35_u8, L"Y"}, {36_u8, L"Z"}, {37_u8, L"a"}, {38_u8, L"b"}, {39_u8, L"c"},
    {40_u8, L"d"}, {41_u8, L"e"}, {42_u8, L"f"}, {43_u8, L"g"}, {44_u8, L"h"}, {45_u8, L"i"}, {46_u8, L"j"}, {47_u8, L"k"},
    {48_u8, L"l"}, {49_u8, L"m"}, {50_u8, L"n"}, {51_u8, L"o"}, {52_u8, L"p"}, {53_u8, L"q"}, {54_u8, L"r"}, {55_u8, L"s"},
    {56_u8, L"t"}, {57_u8, L"u"}, {58_u8, L"v"}, {59_u8, L"w"}, {60_u8, L"x"}, {61_u8, L"y"}, {62_u8, L"z"}, {63_u8, L"\'"},
    {64_u8, L"é"}, {65_u8, L"à"}, {66_u8, L"è"}, {67_u8, L"ù"}, {68_u8, L"â"}, {69_u8, L"ê"}, {70_u8, L"î"}, {71_u8, L"ô"},
    {72_u8, L"û"}, {74_u8, L"-"}, {75_u8, L"×"}
};

const LSString::CharacterSet MENU_GERMAN_CHARSET = {
    { 0_u8, L" "}, { 1_u8, L"0"}, { 2_u8, L"1"}, { 3_u8, L"2"}, { 4_u8, L"3"}, { 5_u8, L"4"}, { 6_u8, L"5"}, { 7_u8, L"6"},
    { 8_u8, L"7"}, { 9_u8, L"8"}, {10_u8, L"9"}, {11_u8, L"A"}, {12_u8, L"B"}, {13_u8, L"C"}, {14_u8, L"D"}, {15_u8, L"E"},
    {16_u8, L"F"}, {17_u8, L"G"}, {18_u8, L"H"}, {19_u8, L"I"}, {20_u8, L"J"}, {21_u8, L"K"}, {22_u8, L"L"}, {23_u8, L"M"},
    {24_u8, L"N"}, {25_u8, L"O"}, {26_u8, L"P"}, {27_u8, L"Q"}, {28_u8, L"R"}, {29_u8, L"S"}, {30_u8, L"T"}, {31_u8, L"U"},
    {32_u8, L"V"}, {33_u8, L"W"}, {34_u8, L"X"}, {35_u8, L"Y"}, {36_u8, L"Z"}, {37_u8, L"a"}, {38_u8, L"b"}, {39_u8, L"c"},
    {40_u8, L"d"}, {41_u8, L"e"}, {42_u8, L"f"}, {43_u8, L"g"}, {44_u8, L"h"}, {45_u8, L"i"}, {46_u8, L"j"}, {47_u8, L"k"},
    {48_u8, L"l"}, {49_u8, L"m"}, {50_u8, L"n"}, {51_u8, L"o"}, {52_u8, L"p"}, {53_u8, L"q"}, {54_u8, L"r"}, {55_u8, L"s"},
    {56_u8, L"t"}, {57_u8, L"u"}, {58_u8, L"v"}, {59_u8, L"w"}, {60_u8, L"x"}, {61_u8, L"y"}, {62_u8, L"z"}, {63_u8, L"Ä"},
    {64_u8, L"Ö"}, {65_u8, L"Ü"}, {66_u8, L"ä"}, {67_u8, L"ö"}, {68_u8, L"ü"}, {69_u8, L"ß"}, {70_u8, L";"}, {71_u8, L"-"},
    {72_u8, L"×"}
};

const LSString::CharacterSet MENU_JAPANESE_CHARSET = {
    {  0_u8, L" "}, {  1_u8, L"0"}, {  2_u8, L"1"}, {  3_u8, L"2"}, {  4_u8, L"3"}, {  5_u8, L"4"}, {  6_u8, L"5"}, {  7_u8, L"6"},
    {  8_u8, L"7"}, {  9_u8, L"8"}, { 10_u8, L"9"}, { 11_u8, L"あ"}, { 12_u8, L"い"}, { 13_u8, L"う"}, { 14_u8, L"え"}, { 15_u8, L"お"},
    { 16_u8, L"か"}, { 17_u8, L"き"}, { 18_u8, L"く"}, { 19_u8, L"け"}, { 20_u8, L"こ"}, { 21_u8, L"さ"}, { 22_u8, L"し"}, { 23_u8, L"す"},
    { 24_u8, L"せ"}, { 25_u8, L"そ"}, { 26_u8, L"た"}, { 27_u8, L"ち"}, { 28_u8, L"つ"}, { 29_u8, L"て"}, { 30_u8, L"と"}, { 31_u8, L"な"},
    { 32_u8, L"に"}, { 33_u8, L"ぬ"}, { 34_u8, L"ね"}, { 35_u8, L"の"}, { 36_u8, L"は"}, { 37_u8, L"ひ"}, { 38_u8, L"ふ"}, { 39_u8, L"へ"},
    { 40_u8, L"ほ"}, { 41_u8, L"ま"}, { 42_u8, L"み"}, { 43_u8, L"む"}, { 44_u8, L"め"}, { 45_u8, L"も"}, { 46_u8, L"や"}, { 47_u8, L"ゆ"},
    { 48_u8, L"よ"}, { 49_u8, L"ら"}, { 50_u8, L"り"}, { 51_u8, L"る"}, { 52_u8, L"れ"}, { 53_u8, L"ろ"}, { 54_u8, L"わ"}, { 55_u8, L"を"},
    { 56_u8, L"ん"}, { 57_u8, L"◉"}, { 58_u8, L"ぃ"}, { 59_u8, L"ぅ"}, { 60_u8, L"ぇ"}, { 61_u8, L"ぉ"}, { 62_u8, L"ゃ"}, { 63_u8, L"ゅ"},
    { 64_u8, L"ょ"}, { 65_u8, L"っ"}, { 66_u8, L"ア"}, { 67_u8, L"イ"}, { 68_u8, L"ウ"}, { 69_u8, L"エ"}, { 70_u8, L"オ"}, { 71_u8, L"カ"},
    { 72_u8, L"キ"}, { 73_u8, L"ク"}, { 74_u8, L"ケ"}, { 75_u8, L"コ"}, { 76_u8, L"サ"}, { 77_u8, L"シ"}, { 78_u8, L"ス"}, { 79_u8, L"セ"},
    { 80_u8, L"ソ"}, { 81_u8, L"タ"}, { 82_u8, L"チ"}, { 83_u8, L"ツ"}, { 84_u8, L"テ"}, { 85_u8, L"ト"}, { 86_u8, L"ナ"}, { 87_u8, L"ニ"},
    { 88_u8, L"ヌ"}, { 89_u8, L"ネ"}, { 90_u8, L"ノ"}, { 91_u8, L"ハ"}, { 92_u8, L"ヒ"}, { 93_u8, L"フ"}, { 94_u8, L"ヘ"}, { 95_u8, L"ホ"},
    { 96_u8, L"マ"}, { 97_u8, L"ミ"}, { 98_u8, L"ム"}, { 99_u8, L"メ"}, {100_u8, L"モ"}, {101_u8, L"ヤ"}, {102_u8, L"ユ"}, {103_u8, L"ヨ"},
    {104_u8, L"ラ"}, {105_u8, L"リ"}, {106_u8, L"ル"}, {107_u8, L"レ"}, {108_u8, L"ロ"}, {109_u8, L"ワ"}, {110_u8, L"ヲ"}, {111_u8, L"ン"},
    {112_u8, L"ァ"}, {113_u8, L"ィ"}, {114_u8, L"ゥ"}, {115_u8, L"ェ"}, {116_u8, L"ォ"}, {117_u8, L"ャ"}, {118_u8, L"ュ"}, {119_u8, L"ョ"},
    {120_u8, L"ッ"}, {121_u8, L"、"}, {122_u8, L"。"}, {123_u8, L"゛"}, {124_u8, L"゜"}, {125_u8, L"ー"}, {126_u8, L"!"}, {127_u8, L"?"},
    {128_u8, L"×"}, {129_u8, L"・"}, {130_u8, L"["}, {131_u8, L"]"}, {138_u8, L"H"}, {139_u8, L"M"}, {140_u8, L"P"}, {157_u8, L"「"},
    {158_u8, L"」"}
};

const LSString::DiacriticMap JAPANESE_DIACRITIC_MAP =
{
    {L"゛",{{L"か",L"が"}, {L"き",L"ぎ"}, {L"く",L"ぐ"}, {L"け",L"げ"}, {L"こ",L"ご"}, {L"さ",L"ざ"}, {L"し",L"じ"},
            {L"す",L"ず"}, {L"せ",L"ぜ"}, {L"そ",L"ぞ"}, {L"た",L"だ"}, {L"ち",L"ぢ"}, {L"つ",L"づ"}, {L"て",L"で"},
            {L"と",L"ど"}, {L"は",L"ば"}, {L"ひ",L"び"}, {L"ふ",L"ぶ"}, {L"へ",L"べ"}, {L"ほ",L"ぼ"}, {L"カ",L"ガ"},
            {L"キ",L"ギ"}, {L"ク",L"グ"}, {L"ケ",L"ゲ"}, {L"コ",L"ゴ"}, {L"サ",L"ザ"}, {L"シ",L"ジ"}, {L"ス",L"ズ"},
            {L"セ",L"ゼ"}, {L"ソ",L"ゾ"}, {L"タ",L"ダ"}, {L"チ",L"ヂ"}, {L"ツ",L"ヅ"}, {L"テ",L"デ"}, {L"ト",L"ド"},
            {L"ハ",L"バ"}, {L"ヒ",L"ビ"}, {L"フ",L"ブ"}, {L"ヘ",L"ベ"}, {L"ホ",L"ボ"}, {L"ウ",L"ヴ"}}},
    {L"゜",{{L"は",L"ぱ"}, {L"ひ",L"ぴ"}, {L"ふ",L"ぷ"}, {L"へ",L"ぺ"}, {L"へ",L"ぺ"}, {L"ほ",L"ぽ"}, {L"ハ",L"パ"},
            {L"ヒ",L"ピ"}, {L"フ",L"プ"}, {L"ヘ",L"ペ"}, {L"ホ",L"ポ"}}}
};

const LSString::DiacriticMap DEFAULT_DIACRITIC_MAP = {};

namespace {

// Standard control character names and their offsets from the string
// begin/terminator marker. These hold for every known region (e.g. US
// STRING_BEGIN = 0x55, SET_COLOUR = 0x55 + 19 = 0x68).
const std::vector<std::pair<std::string, int>> CONTROL_CHAR_OFFSETS =
{
    {"STRING_BEGIN",          0},
    {"SELECTION_POINT",       1},
    {"NEWLINE",               2},
    {"PROMPT_YES_OR_NO",      3},
    {"PAUSE_1S_NO_SKIP",      4},
    {"INSERT_NUMBER",         5},
    {"INSERT_SPEAKER_NAME",   6},
    {"STRING_END",            9},
    {"INSERT_ITEM_NAME",     10},
    {"POP_ITEM",             11},
    {"CONTINUE_PROMPT",      13},
    {"NEWLINE_AND_PROMPT",   14},
    {"PAUSE_1S_SKIPPABLE",   15},
    {"PAUSE_1_5S_SKIPPABLE", 16},
    {"PAUSE_2S_SKIPPABLE",   17},
    {"SET_COLOUR",           19},
    {"HYPHENATION_POINT",    20},
    {"BREAKING_SPACE",       21},
    {"BREAK_POINT",          22}
};

bool IsNumeric(const LSString::StringType& value)
{
    return !value.empty() && std::all_of(value.begin(), value.end(),
        [](wchar_t c) { return c >= L'0' && c <= L'9'; });
}

std::vector<CharsetConstant> BuildDefaultConstants(RomOffsets::Region region)
{
    switch (region)
    {
    case RomOffsets::Region::JP:
        return {
            { "CHR_SPACE",              0x00 },
            // The voicing marks are drawn a row above the cursor without advancing it,
            // so they combine with the preceding kana. Japanese has no menu word-wrap.
            { "CHR_MENU_DAKUTEN",       0x7B },
            { "CHR_MENU_HANDAKUTEN",    0x7C },
            { "CHR_MULT",               0x80 },
            { "CHR_ELLIPSIS_DOT",       0xD8 },
            { "CHR_BEGIN_TALK",         0xD9 },
            { "CHR_STR_BEGIN",          0xE9 },
            { "CHR_ARROW_PROMPT",       0xEA },
        };
    case RomOffsets::Region::FR:
        return {
            { "CHR_SPACE",                  0x00 },
            { "CHR_LAST_LETTER",            0x3E },
            { "CHR_MENU_APOSTROPHE",        0x3F },
            { "CHR_ELLIPSIS_DOT",           0x40 },
            { "CHR_BEGIN_TALK",             0x47 },
            { "CHR_DASH",                   0x48 },
            { "CHR_APOSTROPHE",             0x49 },
            { "CHR_MENU_DASH",              0x4A },
            { "CHR_MULT",                   0x4B },
            { "CHR_OPEN_PAREN",             0x4E },
            { "CHR_STR_BEGIN",              0x64 },
            { "CHR_ARROW_PROMPT",           0x65 },
            { "CHR_HYPHENATION_POINT",      0x78 },
            { "CHR_BREAKING_SPACE",         0x79 },
            { "CHR_BREAK_POINT",            0x7A },
            { "CHR_MENU_HYPHENATION_POINT", 0x78 },
            { "CHR_MENU_BREAKING_SPACE",    0x79 },
            { "CHR_MENU_BREAK_POINT",       0x7A },
        };
    case RomOffsets::Region::DE:
        return {
            { "CHR_SPACE",                  0x00 },
            { "CHR_UPPERCASE_S",            0x1D },
            // The German main font is caps-only, so the alphabet ends at uppercase Z.
            { "CHR_LAST_LETTER",            0x24 },
            { "CHR_ELLIPSIS_DOT",           0x26 },
            { "CHR_BEGIN_TALK",             0x2D },
            { "CHR_DASH",                   0x2E },
            { "CHR_SS",                     0x3E },
            { "CHR_STR_BEGIN",              0x41 },
            { "CHR_ARROW_PROMPT",           0x42 },
            { "CHR_MENU_DASH",              0x47 },
            { "CHR_MULT",                   0x48 },
            { "CHR_HYPHENATION_POINT",      0x55 },
            { "CHR_BREAKING_SPACE",         0x56 },
            { "CHR_BREAK_POINT",            0x57 },
            { "CHR_MENU_HYPHENATION_POINT", 0x55 },
            { "CHR_MENU_BREAKING_SPACE",    0x56 },
            { "CHR_MENU_BREAK_POINT",       0x57 },
        };
    case RomOffsets::Region::US:
    case RomOffsets::Region::UK:
    case RomOffsets::Region::US_BETA:
    default:
        return {
            { "CHR_SPACE",                  0x00 },
            { "CHR_ELLIPSIS_DOT",           0x40 },
            { "CHR_BEGIN_TALK",             0x47 },
            { "CHR_DASH",                   0x48 },
            { "CHR_MULT",                   0x55 },
            { "CHR_STR_BEGIN",              0x55 },
            { "CHR_ARROW_PROMPT",           0x56 },
            { "CHR_HYPHENATION_POINT",      0x69 },
            { "CHR_BREAKING_SPACE",         0x6A },
            { "CHR_BREAK_POINT",            0x6B },
            { "CHR_MENU_HYPHENATION_POINT", 0x69 },
            { "CHR_MENU_BREAKING_SPACE",    0x6A },
            { "CHR_MENU_BREAK_POINT",       0x6B },
        };
    }
}

} // namespace

Charsets GetDefaultCharsets(RomOffsets::Region region)
{
    Charsets charsets;
    charsets.main = GetDefaultCharset(region);
    charsets.menu = GetDefaultMenuCharset(region);
    charsets.intro = IntroString::GetDefaultCharset(region);
    charsets.credits = EndCreditString::GetDefaultCharset(region);
    charsets.diacritics = GetDiacriticMap(region);
    charsets.eos_marker = GetEOSChar(region);
    for (const auto& cc : CONTROL_CHAR_OFFSETS)
    {
        charsets.control_chars[cc.first] = std::to_wstring(charsets.eos_marker + cc.second);
    }
    charsets.constants = BuildDefaultConstants(region);
    return charsets;
}

std::string GetCharsetYamlName(RomOffsets::Region region)
{
    switch (region)
    {
    case RomOffsets::Region::JP:
        return "jp";
    case RomOffsets::Region::FR:
        return "fr";
    case RomOffsets::Region::DE:
        return "de";
    case RomOffsets::Region::US:
    case RomOffsets::Region::UK:
    case RomOffsets::Region::US_BETA:
    default:
        return "en";
    }
}

bool LoadCharsetsFromYaml(const std::filesystem::path& path, Charsets& charsets)
{
    try
    {
        std::ifstream ifs(path);
        if (!ifs.good())
        {
            return false;
        }
        YAML::Node root = YAML::Load(ifs);
        if (!root.IsMap())
        {
            return false;
        }
        auto load_charset = [&root](const char* name, LSString::CharacterSet& out)
        {
            const auto& node = root[name];
            if (node && node.IsMap())
            {
                out.clear();
                for (const auto& entry : node)
                {
                    out[static_cast<uint8_t>(entry.first.as<int>())] = utf8_to_wstr(entry.second.as<std::string>());
                }
            }
        };
        load_charset("Main", charsets.main);
        load_charset("Menu", charsets.menu);
        load_charset("Intro", charsets.intro);
        load_charset("Credits", charsets.credits);
        const auto& diacritics = root["Diacritics"];
        if (diacritics && diacritics.IsMap())
        {
            charsets.diacritics.clear();
            for (const auto& mark : diacritics)
            {
                auto& combinations = charsets.diacritics[utf8_to_wstr(mark.first.as<std::string>())];
                for (const auto& combo : mark.second)
                {
                    combinations[utf8_to_wstr(combo.first.as<std::string>())] = utf8_to_wstr(combo.second.as<std::string>());
                }
            }
        }
        const auto& control_chars = root["ControlChars"];
        if (control_chars && control_chars.IsMap())
        {
            charsets.control_chars.clear();
            for (const auto& entry : control_chars)
            {
                charsets.control_chars[entry.first.as<std::string>()] = utf8_to_wstr(entry.second.as<std::string>());
            }
            auto eos = charsets.control_chars.find("STRING_BEGIN");
            if (eos != charsets.control_chars.end() && IsNumeric(eos->second))
            {
                charsets.eos_marker = static_cast<uint8_t>(std::stoi(eos->second));
            }
        }
        const auto& constants = root["Constants"];
        if (constants && constants.IsMap())
        {
            charsets.constants.clear();
            for (const auto& entry : constants)
            {
                CharsetConstant constant;
                // Constants are listed in the YAML without the "CHR_" prefix.
                constant.name = entry.first.as<std::string>();
                if (constant.name.rfind("CHR_", 0) != 0)
                {
                    constant.name = "CHR_" + constant.name;
                }
                constant.value = entry.second.as<int>();
                charsets.constants.push_back(constant);
                // The string-begin marker doubles as the Huffman EOS marker.
                if (constant.name == "CHR_STR_BEGIN")
                {
                    charsets.eos_marker = static_cast<uint8_t>(constant.value);
                }
            }
        }
        return true;
    }
    catch (const std::exception& e)
    {
        Debug(std::string("Failed to parse charset YAML file '") + path.string() + "': " + e.what());
    }
    return false;
}

bool SaveCharsetsToYaml(const std::filesystem::path& path, const Charsets& charsets)
{
    try
    {
        YAML::Emitter out;
        auto emit_charset = [&out](const char* name, const LSString::CharacterSet& charset)
        {
            std::map<int, std::string> sorted;
            for (const auto& entry : charset)
            {
                sorted[entry.first] = wstr_to_utf8(entry.second);
            }
            out << YAML::Key << name << YAML::Value << YAML::BeginMap;
            for (const auto& entry : sorted)
            {
                out << YAML::Key << entry.first << YAML::Value << entry.second;
            }
            out << YAML::EndMap;
        };
        out << YAML::BeginMap;
        emit_charset("Main", charsets.main);
        emit_charset("Menu", charsets.menu);
        emit_charset("Intro", charsets.intro);
        emit_charset("Credits", charsets.credits);
        if (!charsets.diacritics.empty())
        {
            std::map<std::string, std::map<std::string, std::string>> sorted;
            for (const auto& mark : charsets.diacritics)
            {
                auto& combinations = sorted[wstr_to_utf8(mark.first)];
                for (const auto& combo : mark.second)
                {
                    combinations[wstr_to_utf8(combo.first)] = wstr_to_utf8(combo.second);
                }
            }
            out << YAML::Key << "Diacritics" << YAML::Value << YAML::BeginMap;
            for (const auto& mark : sorted)
            {
                out << YAML::Key << mark.first << YAML::Value << YAML::BeginMap;
                for (const auto& combo : mark.second)
                {
                    out << YAML::Key << combo.first << YAML::Value << combo.second;
                }
                out << YAML::EndMap;
            }
            out << YAML::EndMap;
        }
        // ControlChars and Constants are combined into a single Constants
        // section: the CHR_* code-point symbols the assembler references, listed
        // without the "CHR_" prefix. The remaining in-string control codes are
        // derived from the string marker, so they are not written out.
        if (!charsets.constants.empty())
        {
            out << YAML::Key << "Constants" << YAML::Value << YAML::BeginMap;
            for (const auto& constant : charsets.constants)
            {
                std::string key = constant.name;
                if (key.rfind("CHR_", 0) == 0)
                {
                    key = key.substr(4);
                }
                out << YAML::Key << key << YAML::Value << constant.value;
            }
            out << YAML::EndMap;
        }
        out << YAML::EndMap;
        std::ofstream ofs(path, std::ios::binary);
        if (!ofs.good())
        {
            return false;
        }
        ofs << out.c_str() << std::endl;
        return ofs.good();
    }
    catch (const std::exception& e)
    {
        Debug(std::string("Failed to write charset YAML file '") + path.string() + "': " + e.what());
    }
    return false;
}

} // namespace Charset
} // namespace Landstalker

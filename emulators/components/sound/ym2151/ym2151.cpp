// Yamaha YM2151 (OPM). See ym2151.h for the overview.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>

#include "ym2151.h"

static constexpr u32 FREQ_SH  = 16;     // 16.16 fixed point (frequency)
static constexpr u32 EG_SH    = 16;     // 16.16 fixed point (envelope timing)
static constexpr u32 LFO_SH   = 10;     // 22.10 fixed point (LFO)
static constexpr u32 TIMER_SH = 16;     // 16.16 fixed point (timers)

static constexpr u32 FREQ_MASK = (1 << FREQ_SH) - 1;

static constexpr u32    ENV_BITS = 10;
static constexpr u32    ENV_LEN  = 1 << ENV_BITS;
static constexpr double ENV_STEP = 128.0 / ENV_LEN;

static constexpr s32 MAX_ATT_INDEX = ENV_LEN - 1;
static constexpr s32 MIN_ATT_INDEX = 0;

static constexpr u32 EG_ATT = 4;
static constexpr u32 EG_DEC = 3;
static constexpr u32 EG_SUS = 2;
static constexpr u32 EG_REL = 1;
static constexpr u32 EG_OFF = 0;

static constexpr u32 SIN_BITS = 10;
static constexpr u32 SIN_LEN  = 1 << SIN_BITS;
static constexpr u32 SIN_MASK = SIN_LEN - 1;

static constexpr u32 TL_RES_LEN = 256;  // 8 bit addressing, as on the real chip

static constexpr s32 MAXOUT = +32767;
static constexpr s32 MINOUT = -32768;

// 13 sine amplitude bits + 2 sign bits on the Y axis, TL_RES_LEN on the X axis.
static constexpr u32 TL_TAB_LEN = 13 * 2 * TL_RES_LEN;

static constexpr u32 ENV_QUIET = TL_TAB_LEN >> 3;

static constexpr u32 RATE_STEPS = 8;
static constexpr u8 egInc[19 * RATE_STEPS] = {

/*cycle:0 1  2 3  4 5  6 7*/

/* 0 */ 0,1, 0,1, 0,1, 0,1, /* rates 00..11 0 (increment by 0 or 1) */
/* 1 */ 0,1, 0,1, 1,1, 0,1, /* rates 00..11 1 */
/* 2 */ 0,1, 1,1, 0,1, 1,1, /* rates 00..11 2 */
/* 3 */ 0,1, 1,1, 1,1, 1,1, /* rates 00..11 3 */

/* 4 */ 1,1, 1,1, 1,1, 1,1, /* rate 12 0 (increment by 1) */
/* 5 */ 1,1, 1,2, 1,1, 1,2, /* rate 12 1 */
/* 6 */ 1,2, 1,2, 1,2, 1,2, /* rate 12 2 */
/* 7 */ 1,2, 2,2, 1,2, 2,2, /* rate 12 3 */

/* 8 */ 2,2, 2,2, 2,2, 2,2, /* rate 13 0 (increment by 2) */
/* 9 */ 2,2, 2,4, 2,2, 2,4, /* rate 13 1 */
/*10 */ 2,4, 2,4, 2,4, 2,4, /* rate 13 2 */
/*11 */ 2,4, 4,4, 2,4, 4,4, /* rate 13 3 */

/*12 */ 4,4, 4,4, 4,4, 4,4, /* rate 14 0 (increment by 4) */
/*13 */ 4,4, 4,8, 4,4, 4,8, /* rate 14 1 */
/*14 */ 4,8, 4,8, 4,8, 4,8, /* rate 14 2 */
/*15 */ 4,8, 8,8, 4,8, 8,8, /* rate 14 3 */

/*16 */ 8,8, 8,8, 8,8, 8,8, /* rates 15 0, 15 1, 15 2, 15 3 (increment by 8) */
/*17 */ 16,16,16,16,16,16,16,16, /* rates 15 2, 15 3 for attack */
/*18 */ 0,0, 0,0, 0,0, 0,0, /* infinity rates for attack and decay(s) */
};


#define O(a) (a*RATE_STEPS)

// There is deliberately no O(17) here; the attack path indexes it directly.
// Envelope generator rates: 32 dummy + 64 rates + 32 RKS.
static constexpr u8 egRateSelect[32+64+32]={
/* 32 dummy (infinite time) rates */
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),

/* rates 00-11 */
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),

/* rate 12 */
O( 4),O( 5),O( 6),O( 7),

/* rate 13 */
O( 8),O( 9),O(10),O(11),

/* rate 14 */
O(12),O(13),O(14),O(15),

/* rate 15 */
O(16),O(16),O(16),O(16),

/* 32 dummy rates (same as 15 3) */
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16),
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16),
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16),
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16)

};
#undef O

/*rate  0,    1,    2,   3,   4,   5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15*/
/*shift 11,   10,   9,   8,   7,   6,  5,  4,  3,  2, 1,  0,  0,  0,  0,  0 */
/*mask  2047, 1023, 511, 255, 127, 63, 31, 15, 7,  3, 1,  0,  0,  0,  0,  0 */

#define O(a) (a*1)
// Envelope generator counter shifts: 32 dummy + 64 rates + 32 RKS.
static constexpr u8 egRateShift[32+64+32]={
/* 32 infinite time rates */
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),


/* rates 00-11 */
O(11),O(11),O(11),O(11),
O(10),O(10),O(10),O(10),
O( 9),O( 9),O( 9),O( 9),
O( 8),O( 8),O( 8),O( 8),
O( 7),O( 7),O( 7),O( 7),
O( 6),O( 6),O( 6),O( 6),
O( 5),O( 5),O( 5),O( 5),
O( 4),O( 4),O( 4),O( 4),
O( 3),O( 3),O( 3),O( 3),
O( 2),O( 2),O( 2),O( 2),
O( 1),O( 1),O( 1),O( 1),
O( 0),O( 0),O( 0),O( 0),

/* rate 12 */
O( 0),O( 0),O( 0),O( 0),

/* rate 13 */
O( 0),O( 0),O( 0),O( 0),

/* rate 14 */
O( 0),O( 0),O( 0),O( 0),

/* rate 15 */
O( 0),O( 0),O( 0),O( 0),

/* 32 dummy rates (same as 15 3) */
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0)

};
#undef O

// DT2 offset from the base note, in cents, as an index into the frequency
// delta table. From page 22 of the user's manual, divided by 1.5625:
//   DT2=0  DT2=1  DT2=2  DT2=3
//   0      600    781    950
static constexpr u32 dt2Tab[4] = { 0, 384, 500, 608 };

// DT1 offset from the base note in Hertz, converted to phase increments during
// init. The detune table printed in the user's manual is wrong; this one was
// verified against the real chip.
static constexpr u8 dt1Tab[4*32] = {
/* DT1=0 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

/* DT1=1 */
  0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
  2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8, 8,

/* DT1=2 */
  1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5,
  5, 6, 6, 7, 8, 8, 9,10,11,12,13,14,16,16,16,16,

/* DT1=3 */
  2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7,
  8, 8, 9,10,11,12,13,14,16,17,19,20,22,22,22,22
};

// Phase increments read out of the real chip's internal ROM, in 10.10 format.
static constexpr u16 phaseIncRom[768]={
1299,1300,1301,1302,1303,1304,1305,1306,1308,1309,1310,1311,1313,1314,1315,1316,
1318,1319,1320,1321,1322,1323,1324,1325,1327,1328,1329,1330,1332,1333,1334,1335,
1337,1338,1339,1340,1341,1342,1343,1344,1346,1347,1348,1349,1351,1352,1353,1354,
1356,1357,1358,1359,1361,1362,1363,1364,1366,1367,1368,1369,1371,1372,1373,1374,
1376,1377,1378,1379,1381,1382,1383,1384,1386,1387,1388,1389,1391,1392,1393,1394,
1396,1397,1398,1399,1401,1402,1403,1404,1406,1407,1408,1409,1411,1412,1413,1414,
1416,1417,1418,1419,1421,1422,1423,1424,1426,1427,1429,1430,1431,1432,1434,1435,
1437,1438,1439,1440,1442,1443,1444,1445,1447,1448,1449,1450,1452,1453,1454,1455,
1458,1459,1460,1461,1463,1464,1465,1466,1468,1469,1471,1472,1473,1474,1476,1477,
1479,1480,1481,1482,1484,1485,1486,1487,1489,1490,1492,1493,1494,1495,1497,1498,
1501,1502,1503,1504,1506,1507,1509,1510,1512,1513,1514,1515,1517,1518,1520,1521,
1523,1524,1525,1526,1528,1529,1531,1532,1534,1535,1536,1537,1539,1540,1542,1543,
1545,1546,1547,1548,1550,1551,1553,1554,1556,1557,1558,1559,1561,1562,1564,1565,
1567,1568,1569,1570,1572,1573,1575,1576,1578,1579,1580,1581,1583,1584,1586,1587,
1590,1591,1592,1593,1595,1596,1598,1599,1601,1602,1604,1605,1607,1608,1609,1610,
1613,1614,1615,1616,1618,1619,1621,1622,1624,1625,1627,1628,1630,1631,1632,1633,
1637,1638,1639,1640,1642,1643,1645,1646,1648,1649,1651,1652,1654,1655,1656,1657,
1660,1661,1663,1664,1666,1667,1669,1670,1672,1673,1675,1676,1678,1679,1681,1682,
1685,1686,1688,1689,1691,1692,1694,1695,1697,1698,1700,1701,1703,1704,1706,1707,
1709,1710,1712,1713,1715,1716,1718,1719,1721,1722,1724,1725,1727,1728,1730,1731,
1734,1735,1737,1738,1740,1741,1743,1744,1746,1748,1749,1751,1752,1754,1755,1757,
1759,1760,1762,1763,1765,1766,1768,1769,1771,1773,1774,1776,1777,1779,1780,1782,
1785,1786,1788,1789,1791,1793,1794,1796,1798,1799,1801,1802,1804,1806,1807,1809,
1811,1812,1814,1815,1817,1819,1820,1822,1824,1825,1827,1828,1830,1832,1833,1835,
1837,1838,1840,1841,1843,1845,1846,1848,1850,1851,1853,1854,1856,1858,1859,1861,
1864,1865,1867,1868,1870,1872,1873,1875,1877,1879,1880,1882,1884,1885,1887,1888,
1891,1892,1894,1895,1897,1899,1900,1902,1904,1906,1907,1909,1911,1912,1914,1915,
1918,1919,1921,1923,1925,1926,1928,1930,1932,1933,1935,1937,1939,1940,1942,1944,
1946,1947,1949,1951,1953,1954,1956,1958,1960,1961,1963,1965,1967,1968,1970,1972,
1975,1976,1978,1980,1982,1983,1985,1987,1989,1990,1992,1994,1996,1997,1999,2001,
2003,2004,2006,2008,2010,2011,2013,2015,2017,2019,2021,2022,2024,2026,2028,2029,
2032,2033,2035,2037,2039,2041,2043,2044,2047,2048,2050,2052,2054,2056,2058,2059,
2062,2063,2065,2067,2069,2071,2073,2074,2077,2078,2080,2082,2084,2086,2088,2089,
2092,2093,2095,2097,2099,2101,2103,2104,2107,2108,2110,2112,2114,2116,2118,2119,
2122,2123,2125,2127,2129,2131,2133,2134,2137,2139,2141,2142,2145,2146,2148,2150,
2153,2154,2156,2158,2160,2162,2164,2165,2168,2170,2172,2173,2176,2177,2179,2181,
2185,2186,2188,2190,2192,2194,2196,2197,2200,2202,2204,2205,2208,2209,2211,2213,
2216,2218,2220,2222,2223,2226,2227,2230,2232,2234,2236,2238,2239,2242,2243,2246,
2249,2251,2253,2255,2256,2259,2260,2263,2265,2267,2269,2271,2272,2275,2276,2279,
2281,2283,2285,2287,2288,2291,2292,2295,2297,2299,2301,2303,2304,2307,2308,2311,
2315,2317,2319,2321,2322,2325,2326,2329,2331,2333,2335,2337,2338,2341,2342,2345,
2348,2350,2352,2354,2355,2358,2359,2362,2364,2366,2368,2370,2371,2374,2375,2378,
2382,2384,2386,2388,2389,2392,2393,2396,2398,2400,2402,2404,2407,2410,2411,2414,
2417,2419,2421,2423,2424,2427,2428,2431,2433,2435,2437,2439,2442,2445,2446,2449,
2452,2454,2456,2458,2459,2462,2463,2466,2468,2470,2472,2474,2477,2480,2481,2484,
2488,2490,2492,2494,2495,2498,2499,2502,2504,2506,2508,2510,2513,2516,2517,2520,
2524,2526,2528,2530,2531,2534,2535,2538,2540,2542,2544,2546,2549,2552,2553,2556,
2561,2563,2565,2567,2568,2571,2572,2575,2577,2579,2581,2583,2586,2589,2590,2593
};


// Noise LFO waveform: 256 samples captured from a real chip out of a much
// longer sequence. It does not repeat every 256 samples on hardware and the
// real algorithm is unknown, so this snapshot is the best available.
//
// Because of the way the LFO output is derived, some of the 0x80 entries could
// really be 0x81 and some of the 0x00 entries could really be 0x01.
static constexpr u8 lfoNoiseWaveform[256] = {
0xFF,0xEE,0xD3,0x80,0x58,0xDA,0x7F,0x94,0x9E,0xE3,0xFA,0x00,0x4D,0xFA,0xFF,0x6A,
0x7A,0xDE,0x49,0xF6,0x00,0x33,0xBB,0x63,0x91,0x60,0x51,0xFF,0x00,0xD8,0x7F,0xDE,
0xDC,0x73,0x21,0x85,0xB2,0x9C,0x5D,0x24,0xCD,0x91,0x9E,0x76,0x7F,0x20,0xFB,0xF3,
0x00,0xA6,0x3E,0x42,0x27,0x69,0xAE,0x33,0x45,0x44,0x11,0x41,0x72,0x73,0xDF,0xA2,

0x32,0xBD,0x7E,0xA8,0x13,0xEB,0xD3,0x15,0xDD,0xFB,0xC9,0x9D,0x61,0x2F,0xBE,0x9D,
0x23,0x65,0x51,0x6A,0x84,0xF9,0xC9,0xD7,0x23,0xBF,0x65,0x19,0xDC,0x03,0xF3,0x24,
0x33,0xB6,0x1E,0x57,0x5C,0xAC,0x25,0x89,0x4D,0xC5,0x9C,0x99,0x15,0x07,0xCF,0xBA,
0xC5,0x9B,0x15,0x4D,0x8D,0x2A,0x1E,0x1F,0xEA,0x2B,0x2F,0x64,0xA9,0x50,0x3D,0xAB,

0x50,0x77,0xE9,0xC0,0xAC,0x6D,0x3F,0xCA,0xCF,0x71,0x7D,0x80,0xA6,0xFD,0xFF,0xB5,
0xBD,0x6F,0x24,0x7B,0x00,0x99,0x5D,0xB1,0x48,0xB0,0x28,0x7F,0x80,0xEC,0xBF,0x6F,
0x6E,0x39,0x90,0x42,0xD9,0x4E,0x2E,0x12,0x66,0xC8,0xCF,0x3B,0x3F,0x10,0x7D,0x79,
0x00,0xD3,0x1F,0x21,0x93,0x34,0xD7,0x19,0x22,0xA2,0x08,0x20,0xB9,0xB9,0xEF,0x51,

0x99,0xDE,0xBF,0xD4,0x09,0x75,0xE9,0x8A,0xEE,0xFD,0xE4,0x4E,0x30,0x17,0xDF,0xCE,
0x11,0xB2,0x28,0x35,0xC2,0x7C,0x64,0xEB,0x91,0x5F,0x32,0x0C,0x6E,0x00,0xF9,0x92,
0x19,0xDB,0x8F,0xAB,0xAE,0xD6,0x12,0xC4,0x26,0x62,0xCE,0xCC,0x0A,0x03,0xE7,0xDD,
0xE2,0x4D,0x8A,0xA6,0x46,0x95,0x0F,0x8F,0xF5,0x15,0x97,0x32,0xD4,0x28,0x1E,0x55
};


// These three tables are shared by every chip and depend only on compile-time
// constants. tlTab and sinTab need pow/log/sin, which are not constexpr, so
// they are filled in during static initialisation instead; the result is still
// immutable for the lifetime of the program.
static std::array<s32, TL_TAB_LEN> makeTlTab() {
    std::array<s32, TL_TAB_LEN> tab{};

    for (u32 x = 0; x < TL_RES_LEN; x++) {
        double m = (1 << 16) / pow(2, (x + 1) * (ENV_STEP / 4.0) / 8.0);
        m = floor(m);

        // The (x+1) keeps this below (1<<16), so it fits in 16 bits.
        s32 n = static_cast<s32>(m);    // 16 bits
        n >>= 4;                        // 12 bits
        n = (n & 1) ? (n >> 1) + 1 : n >> 1;    // round to closest, 11 bits
        n <<= 2;                        // 13 bits, as on the real chip

        tab[x * 2 + 0] = n;
        tab[x * 2 + 1] = -tab[x * 2 + 0];

        for (u32 i = 1; i < 13; i++) {
            tab[x * 2 + 0 + i * 2 * TL_RES_LEN] =  tab[x * 2 + 0] >> i;
            tab[x * 2 + 1 + i * 2 * TL_RES_LEN] = -tab[x * 2 + 0 + i * 2 * TL_RES_LEN];
        }
    }

    return tab;
}

// Sine waveform table on a 'decibel' scale.
static std::array<u32, SIN_LEN> makeSinTab() {
    static constexpr double PI = 3.14159265358979323846;
    std::array<u32, SIN_LEN> tab{};

    for (u32 i = 0; i < SIN_LEN; i++) {
        // Non-standard sine, verified on the real chip. The ((i*2)+1) keeps it
        // away from zero.
        double m = sin(((i * 2) + 1) * PI / SIN_LEN);

        // Convert to 'decibels'.
        double o = (m > 0.0) ? 8 * log(1.0 / m) / log(2.0)
                             : 8 * log(-1.0 / m) / log(2.0);
        o = o / (ENV_STEP / 4);

        s32 n = static_cast<s32>(2.0 * o);
        n = (n & 1) ? (n >> 1) + 1 : n >> 1;    // round to closest

        tab[i] = n * 2 + (m >= 0.0 ? 0 : 1);
    }

    return tab;
}

// Translates D1L to a volume index, for the 16 D1L levels.
static constexpr std::array<u32, 16> makeSustainLevelTab() {
    std::array<u32, 16> tab{};

    // Every 3 'dB', except that all bits set means 45+48 'dB'.
    for (u32 i = 0; i < 16; i++) {
        tab[i] = static_cast<u32>((i != 15 ? i : i + 16) * (4.0 / ENV_STEP));
    }

    return tab;
}

static const std::array<s32, TL_TAB_LEN> tlTab = makeTlTab();
static const std::array<u32, SIN_LEN> sinTab = makeSinTab();
static constexpr std::array<u32, 16> sustainLevelTab = makeSustainLevelTab();

void Ym2151::initChipTables() {
    double scaler = (m_clock / 64.0) / m_sampleRate;

    // Hertz values for notes from c-0 to b-7, with 64 'cents' per note (each
    // 100/64, that is 1.5625, of a real cent). i*100/64/1200 equals i/768.
    //
    // The real chip works in 10.10 fixed point, and the phaseIncRom values are
    // already in that format, hence the -10.
    double mult = 1 << (FREQ_SH - 10);

    for (u32 i = 0; i < 768; i++) {
        double phaseinc = phaseIncRom[i] * scaler;

        // Octave 2 is the reference octave. The mask adjusts to X.10 fixed point.
        m_freq[768 + 2 * 768 + i] = static_cast<u32>(phaseinc * mult) & 0xffffffc0;

        for (u32 j = 0; j < 2; j++) {       // octaves 0 and 1
            m_freq[768 + j * 768 + i] = (m_freq[768 + 2 * 768 + i] >> (2 - j)) & 0xffffffc0;
        }
        for (u32 j = 3; j < 8; j++) {       // octaves 3 to 7
            m_freq[768 + j * 768 + i] = m_freq[768 + 2 * 768 + i] << (j - 2);
        }
    }

    // Octave -1, all equal to octave 0, _KC_00_, _KF_00_.
    for (u32 i = 0; i < 768; i++) {
        m_freq[i] = m_freq[768];
    }

    // Octaves 8 and 9, all equal to octave 7, _KC_14_, _KF_63_.
    for (u32 j = 8; j < 10; j++) {
        for (u32 i = 0; i < 768; i++) {
            m_freq[768 + j * 768 + i] = m_freq[768 + 8 * 768 - 1];
        }
    }

    mult = 1 << FREQ_SH;
    for (u32 j = 0; j < 4; j++) {
        for (u32 i = 0; i < 32; i++) {
            double hz = (dt1Tab[j * 32 + i] * (m_clock / 64.0)) / (1 << 20);
            double phaseinc = (hz * SIN_LEN) / m_sampleRate;

            m_dt1Freq[(j + 0) * 32 + i] = phaseinc * mult;
            m_dt1Freq[(j + 4) * 32 + i] = -m_dt1Freq[(j + 0) * 32 + i];
        }
    }

    // Timer deltas, from pages 15 and 16 of the user's manual.
    mult = 1 << TIMER_SH;
    for (u32 i = 0; i < 1024; i++) {
        // Number of samples the timer period takes, in fixed point.
        m_timerATable[i] = (64.0 * (1024.0 - i) / m_clock) * m_sampleRate * mult;
    }
    for (u32 i = 0; i < 256; i++) {
        m_timerBTable[i] = (1024.0 * (256.0 - i) / m_clock) * m_sampleRate * mult;
    }

    scaler = (m_clock / 64.0) / m_sampleRate;
    for (u32 i = 0; i < 32; i++) {
        u32 j = (i != 31) ? i : 30;         // rates 30 and 31 are the same
        j = 32 - j;
        // Number of samples per one shift of the shift register.
        j = static_cast<u32>(65536.0 / (j * 32.0));
        m_noiseTable[i] = j * 64 * scaler;
    }
}

void Ym2151::keyOn(Operator* op, u32 keySet) {
    if (!op->keyOn) {
        op->phase = 0;
        op->egState = EG_ATT;
        op->volume += (~op->volume *
                       egInc[op->egSelectAttack + ((m_egCnt >> op->egShiftAttack) & 7)]) >> 4;
        if (op->volume <= MIN_ATT_INDEX) {
            op->volume = MIN_ATT_INDEX;
            op->egState = EG_DEC;
        }
    }
    op->keyOn |= keySet;
}

void Ym2151::keyOff(Operator* op, u32 keyClear) {
    if (op->keyOn) {
        op->keyOn &= keyClear;
        if (!op->keyOn && op->egState > EG_REL) {
            op->egState = EG_REL;
        }
    }
}

void Ym2151::envelopeKonKoff(Operator* op, u8 v) {
    // Bits 3, 5, 4, 6 select M1, M2, C1, C2 respectively.
    static constexpr u8 slotBit[4] = { 0x08, 0x20, 0x10, 0x40 };

    for (u32 i = 0; i < 4; i++) {
        if (v & slotBit[i]) {
            keyOn(op + i, 1);
        } else {
            keyOff(op + i, ~1u);
        }
    }
}


// MEM is simply a one sample delay.
void Ym2151::setConnect(Operator* om1, u32 cha, u32 v) {
    Operator* om2 = om1 + 1;
    Operator* oc1 = om1 + 2;

    switch (v & 7) {
    case 0:
        // M1---C1---MEM---M2---C2---OUT
        om1->connect = &m_c1;
        oc1->connect = &m_mem;
        om2->connect = &m_c2;
        om1->memConnect = &m_m2;
        break;

    case 1:
        // M1------+-MEM---M2---C2---OUT
        //      C1-+
        om1->connect = &m_mem;
        oc1->connect = &m_mem;
        om2->connect = &m_c2;
        om1->memConnect = &m_m2;
        break;

    case 2:
        // M1-----------------+-C2---OUT
        //      C1---MEM---M2-+
        om1->connect = &m_c2;
        oc1->connect = &m_mem;
        om2->connect = &m_c2;
        om1->memConnect = &m_m2;
        break;

    case 3:
        // M1---C1---MEM------+-C2---OUT
        //                 M2-+
        om1->connect = &m_c1;
        oc1->connect = &m_mem;
        om2->connect = &m_c2;
        om1->memConnect = &m_c2;
        break;

    case 4:
        // M1---C1-+-OUT
        // M2---C2-+     MEM is unused, so park it somewhere harmless.
        om1->connect = &m_c1;
        oc1->connect = &m_chanOut[cha];
        om2->connect = &m_c2;
        om1->memConnect = &m_mem;
        break;

    case 5:
        //    +----C1----+
        // M1-+-MEM---M2-+-OUT
        //    +----C2----+
        om1->connect = nullptr;         // special mark for this algorithm
        oc1->connect = &m_chanOut[cha];
        om2->connect = &m_chanOut[cha];
        om1->memConnect = &m_m2;
        break;

    case 6:
        // M1---C1-+
        //      M2-+-OUT
        //      C2-+     MEM is unused.
        om1->connect = &m_c1;
        oc1->connect = &m_chanOut[cha];
        om2->connect = &m_chanOut[cha];
        om1->memConnect = &m_mem;
        break;

    case 7:
        // M1-+
        // C1-+-OUT
        // M2-+
        // C2-+          MEM is unused.
        om1->connect = &m_chanOut[cha];
        oc1->connect = &m_chanOut[cha];
        om2->connect = &m_chanOut[cha];
        om1->memConnect = &m_mem;
        break;
    }
}

// The operators hold raw pointers into the mixing scratch, so they have to be
// rebuilt after loading a state (and after the object is moved or copied).
void Ym2151::refreshConnections() {
    for (u32 chan = 0; chan < 8; chan++) {
        setConnect(&m_oper[chan * 4], chan, m_connect[chan]);
    }
}

// Recomputes the envelope rate shifts and selectors for all four operators of
// one channel, after the key code changed.
void Ym2151::refreshEg(Operator* op) {
    u32 kc = op->kc;

    for (u32 i = 0; i < 4; i++, op++) {
        // v = 32 + 2*RATE + RKS, up to 126.
        u32 v = kc >> op->keyScale;

        if ((op->attackRate + v) < 32 + 62) {
            op->egShiftAttack  = egRateShift [op->attackRate + v];
            op->egSelectAttack = egRateSelect[op->attackRate + v];
        } else {
            op->egShiftAttack  = 0;
            op->egSelectAttack = 17 * RATE_STEPS;
        }
        op->egShiftDecay    = egRateShift [op->decayRate   + v];
        op->egSelectDecay   = egRateSelect[op->decayRate   + v];
        op->egShiftSustain  = egRateShift [op->sustainRate + v];
        op->egSelectSustain = egRateSelect[op->sustainRate + v];
        op->egShiftRelease  = egRateShift [op->releaseRate + v];
        op->egSelectRelease = egRateSelect[op->releaseRate + v];
    }
}


void Ym2151::write(u8 reg, u8 value) {
    const u32 r = reg;
    u32 v = value;

    Operator* op = &m_oper[(r & 0x07) * 4 + ((r & 0x18) >> 3)];

    switch (r & 0xe0) {
    case 0x00:
        switch (r) {
        case 0x01:      // LFO reset (bit 1), test register (other bits)
            m_test = v;
            if (v & 2) {
                m_lfoPhase = 0;
            }
            break;

        case 0x08:
            envelopeKonKoff(&m_oper[(v & 7) * 4], v);
            break;

        case 0x0f:      // noise mode enable, noise period
            m_noise = v;
            m_noisePeriod = m_noiseTable[v & 0x1f];
            break;

        case 0x10:      // timer A high
            m_timerAIndex = (m_timerAIndex & 0x003) | (v << 2);
            break;

        case 0x11:      // timer A low
            m_timerAIndex = (m_timerAIndex & 0x3fc) | (v & 3);
            break;

        case 0x12:      // timer B
            m_timerBIndex = v;
            break;

        case 0x14:      // CSM, IRQ flag reset, IRQ enable, timer start/stop
            m_irqEnable = v;    // bit 3 timer B, bit 2 timer A, bit 7 CSM

            if (v & 0x20) {     // reset the timer B IRQ flag
                u32 oldstate = m_status & 3;
                m_status &= 0xfd;
                if (oldstate == 2 && m_irqHandler) {
                    m_irqHandler(false);
                }
            }

            if (v & 0x10) {     // reset the timer A IRQ flag
                u32 oldstate = m_status & 3;
                m_status &= 0xfe;
                if (oldstate == 1 && m_irqHandler) {
                    m_irqHandler(false);
                }
            }

            if (v & 0x02) {     // load and start timer B
                if (!m_timerBEnabled) {
                    m_timerBEnabled = true;
                    m_timerBValue = m_timerBTable[m_timerBIndex];
                }
            } else {
                m_timerBEnabled = false;
            }

            if (v & 0x01) {     // load and start timer A
                if (!m_timerAEnabled) {
                    m_timerAEnabled = true;
                    m_timerAValue = m_timerATable[m_timerAIndex];
                }
            } else {
                m_timerAEnabled = false;
            }
            break;

        case 0x18:      // LFO frequency
            m_lfoOverflow   = (1 << ((15 - (v >> 4)) + 3)) * (1 << LFO_SH);
            m_lfoCounterAdd = 0x10 + (v & 0x0f);
            break;

        case 0x19:      // PMD (bit 7 == 1) or AMD (bit 7 == 0)
            if (v & 0x80) {
                m_pmd = v & 0x7f;
            } else {
                m_amd = v & 0x7f;
            }
            break;

        case 0x1b:      // CT2, CT1, LFO waveform
            m_ct = v >> 6;
            m_lfoWaveform = v & 3;
            break;

        default:
            break;
        }
        break;

    case 0x20:
        op = &m_oper[(r & 7) * 4];
        switch (r & 0x18) {
        case 0x00:      // RL enable, feedback, connection
            op->feedbackShift = ((v >> 3) & 7) ? ((v >> 3) & 7) + 6 : 0;
            m_pan[(r & 7) * 2 + 0] = (v & 0x40) ? ~0u : 0;
            m_pan[(r & 7) * 2 + 1] = (v & 0x80) ? ~0u : 0;
            m_connect[r & 7] = v & 7;
            setConnect(op, r & 7, v & 7);
            break;

        case 0x08: {    // key code
            v &= 0x7f;
            if (v == op->kc) {
                break;
            }

            u32 kcChannel = (v - (v >> 2)) * 64;
            kcChannel += 768;
            kcChannel |= (op->kcIndex & 63);

            u32 kc = v >> 2;
            for (u32 i = 0; i < 4; i++) {
                op[i].kc = v;
                op[i].kcIndex = kcChannel;
                op[i].dt1 = m_dt1Freq[op[i].dt1Index + kc];
                op[i].freq = ((m_freq[kcChannel + op[i].dt2] + op[i].dt1) * op[i].multiple) >> 1;
            }

            refreshEg(op);
            break;
        }

        case 0x10: {    // key fraction
            v >>= 2;
            if (v == (op->kcIndex & 63)) {
                break;
            }

            u32 kcChannel = v | (op->kcIndex & ~63u);
            for (u32 i = 0; i < 4; i++) {
                op[i].kcIndex = kcChannel;
                op[i].freq = ((m_freq[kcChannel + op[i].dt2] + op[i].dt1) * op[i].multiple) >> 1;
            }
            break;
        }

        case 0x18:      // PMS, AMS
            op->pms = (v >> 4) & 7;
            op->ams = v & 3;
            break;
        }
        break;

    case 0x40: {        // DT1, MUL
        u32 oldDt1Index = op->dt1Index;
        u32 oldMultiple = op->multiple;

        op->dt1Index = (v & 0x70) << 1;
        op->multiple = (v & 0x0f) ? (v & 0x0f) << 1 : 1;

        if (oldDt1Index != op->dt1Index) {
            op->dt1 = m_dt1Freq[op->dt1Index + (op->kc >> 2)];
        }
        if (oldDt1Index != op->dt1Index || oldMultiple != op->multiple) {
            op->freq = ((m_freq[op->kcIndex + op->dt2] + op->dt1) * op->multiple) >> 1;
        }
        break;
    }

    case 0x60:          // TL, 7 bits
        op->totalLevel = (v & 0x7f) << (ENV_BITS - 7);
        break;

    case 0x80: {        // KS, AR
        u32 oldKeyScale  = op->keyScale;
        u32 oldAttackRate = op->attackRate;

        op->keyScale   = 5 - (v >> 6);
        op->attackRate = (v & 0x1f) ? 32 + ((v & 0x1f) << 1) : 0;

        if (op->attackRate != oldAttackRate || op->keyScale != oldKeyScale) {
            if ((op->attackRate + (op->kc >> op->keyScale)) < 32 + 62) {
                op->egShiftAttack  = egRateShift [op->attackRate + (op->kc >> op->keyScale)];
                op->egSelectAttack = egRateSelect[op->attackRate + (op->kc >> op->keyScale)];
            } else {
                op->egShiftAttack  = 0;
                op->egSelectAttack = 17 * RATE_STEPS;
            }
        }

        if (op->keyScale != oldKeyScale) {
            op->egShiftDecay    = egRateShift [op->decayRate   + (op->kc >> op->keyScale)];
            op->egSelectDecay   = egRateSelect[op->decayRate   + (op->kc >> op->keyScale)];
            op->egShiftSustain  = egRateShift [op->sustainRate + (op->kc >> op->keyScale)];
            op->egSelectSustain = egRateSelect[op->sustainRate + (op->kc >> op->keyScale)];
            op->egShiftRelease  = egRateShift [op->releaseRate + (op->kc >> op->keyScale)];
            op->egSelectRelease = egRateSelect[op->releaseRate + (op->kc >> op->keyScale)];
        }
        break;
    }

    case 0xa0:          // LFO AM enable, D1R
        op->amMask    = (v & 0x80) ? ~0u : 0;
        op->decayRate = (v & 0x1f) ? 32 + ((v & 0x1f) << 1) : 0;
        op->egShiftDecay  = egRateShift [op->decayRate + (op->kc >> op->keyScale)];
        op->egSelectDecay = egRateSelect[op->decayRate + (op->kc >> op->keyScale)];
        break;

    case 0xc0: {        // DT2, D2R
        u32 oldDt2 = op->dt2;
        op->dt2 = dt2Tab[v >> 6];
        if (op->dt2 != oldDt2) {
            op->freq = ((m_freq[op->kcIndex + op->dt2] + op->dt1) * op->multiple) >> 1;
        }

        op->sustainRate = (v & 0x1f) ? 32 + ((v & 0x1f) << 1) : 0;
        op->egShiftSustain  = egRateShift [op->sustainRate + (op->kc >> op->keyScale)];
        op->egSelectSustain = egRateSelect[op->sustainRate + (op->kc >> op->keyScale)];
        break;
    }

    case 0xe0:          // D1L, RR
        op->sustainLevel = sustainLevelTab[v >> 4];
        op->releaseRate  = 34 + ((v & 0x0f) << 2);
        op->egShiftRelease  = egRateShift [op->releaseRate + (op->kc >> op->keyScale)];
        op->egSelectRelease = egRateSelect[op->releaseRate + (op->kc >> op->keyScale)];
        break;
    }
}

// Visits every piece of state that belongs in a save state. The operator
// connect/memConnect pointers are left out because refreshConnections() rebuilds
// them, and the lookup tables are left out because they are derived from the
// clock and the sample rate.
template <typename Visit>
void Ym2151::visitState(Visit visit) {
    for (Operator& op : m_oper) {
        visit(op.phase);         visit(op.freq);           visit(op.dt1);
        visit(op.multiple);      visit(op.dt1Index);       visit(op.dt2);
        visit(op.memValue);      visit(op.feedbackShift);  visit(op.feedbackOutCurrent);
        visit(op.feedbackOutPrevious); visit(op.kc);       visit(op.kcIndex);
        visit(op.pms);           visit(op.ams);            visit(op.amMask);
        visit(op.egState);       visit(op.egShiftAttack);  visit(op.egSelectAttack);
        visit(op.totalLevel);    visit(op.volume);         visit(op.egShiftDecay);
        visit(op.egSelectDecay); visit(op.sustainLevel);   visit(op.egShiftSustain);
        visit(op.egSelectSustain); visit(op.egShiftRelease); visit(op.egSelectRelease);
        visit(op.keyOn);         visit(op.keyScale);       visit(op.attackRate);
        visit(op.decayRate);     visit(op.sustainRate);    visit(op.releaseRate);
    }

    visit(m_pan);
    visit(m_egCnt);          visit(m_egTimer);
    visit(m_lfoPhase);       visit(m_lfoTimer);
    visit(m_lfoOverflow);    visit(m_lfoCounter);     visit(m_lfoCounterAdd);
    visit(m_lfoWaveform);    visit(m_amd);            visit(m_pmd);
    visit(m_lfa);            visit(m_lfp);
    visit(m_test);           visit(m_ct);
    visit(m_noise);          visit(m_noiseRng);
    visit(m_noisePhase);     visit(m_noisePeriod);
    visit(m_csmRequest);     visit(m_irqEnable);      visit(m_status);
    visit(m_connect);
    visit(m_timerAEnabled);  visit(m_timerBEnabled);
    visit(m_timerAValue);    visit(m_timerBValue);
    visit(m_timerAIndex);    visit(m_timerBIndex);
}

void Ym2151::saveState(Buffer* buf) {
    visitState(StateWriter{buf});
}

void Ym2151::loadState(Buffer* buf) {
    visitState(StateReader{buf});
    refreshConnections();
}


void Ym2151::init(u32 clock, u32 sampleRate) {
    m_clock = clock;
    setSampleRate(sampleRate);
    reset();
}

void Ym2151::setSampleRate(u32 sampleRate) {
    m_sampleRate = (sampleRate > 0) ? sampleRate : 44100;   // avoid a divide by zero

    initChipTables();

    m_lfoTimerAdd = (1 << LFO_SH) * (m_clock / 64.0) / m_sampleRate;
    m_egTimerAdd  = (1 << EG_SH)  * (m_clock / 64.0) / m_sampleRate;
    m_egTimerOverflow = 3 * (1 << EG_SH);
}

void Ym2151::reset() {
    for (Operator& op : m_oper) {
        op = Operator{};
        op.volume = MAX_ATT_INDEX;
        op.kcIndex = 768;       // the minimum kcIndex value
    }

    m_egTimer = 0;
    m_egCnt   = 0;

    m_lfoTimer    = 0;
    m_lfoCounter  = 0;
    m_lfoPhase    = 0;
    m_lfoWaveform = 0;
    m_pmd = 0;
    m_amd = 0;
    m_lfa = 0;
    m_lfp = 0;

    m_test = 0;

    m_irqEnable = 0;
    m_timerAEnabled = false;
    m_timerBEnabled = false;
    m_timerAValue = 0;
    m_timerBValue = 0;
    m_timerAIndex = 0;
    m_timerBIndex = 0;

    m_noise       = 0;
    m_noiseRng    = 0;
    m_noisePhase  = 0;
    m_noisePeriod = m_noiseTable[0];

    m_csmRequest = 0;
    m_status     = 0;

    write(0x1b, 0);     // only because of the CT1 and CT2 output pins
    write(0x18, 0);     // set the LFO frequency
    for (u32 i = 0x20; i < 0x100; i++) {
        write(i, 0);
    }
}


s32 Ym2151::opCalc(const Operator* op, u32 env, s32 pm) {
    u32 p = (env << 3) +
            sinTab[((static_cast<s32>((op->phase & ~FREQ_MASK) + (pm << 15))) >> FREQ_SH) & SIN_MASK];

    return (p >= TL_TAB_LEN) ? 0 : tlTab[p];
}

s32 Ym2151::opCalc1(const Operator* op, u32 env, s32 pm) {
    s32 i = (op->phase & ~FREQ_MASK) + pm;
    u32 p = (env << 3) + sinTab[(i >> FREQ_SH) & SIN_MASK];

    return (p >= TL_TAB_LEN) ? 0 : tlTab[p];
}


void Ym2151::chanCalc(u32 chan) {
    m_m2 = m_c1 = m_c2 = m_mem = 0;

    Operator* op = &m_oper[chan * 4];    // M1
    *op->memConnect = op->memValue;      // restore the delayed sample into m2 or c2

    u32 am = op->ams ? (m_lfa << (op->ams - 1)) : 0;
    u32 env = volumeCalc(op, am);

    s32 out = op->feedbackOutPrevious + op->feedbackOutCurrent;
    op->feedbackOutPrevious = op->feedbackOutCurrent;

    if (!op->connect) {
        m_mem = m_c1 = m_c2 = op->feedbackOutPrevious;      // algorithm 5
    } else {
        *op->connect = op->feedbackOutPrevious;
    }

    op->feedbackOutCurrent = 0;
    if (env < ENV_QUIET) {
        if (!op->feedbackShift) {
            out = 0;
        }
        op->feedbackOutCurrent = opCalc1(op, env, out << op->feedbackShift);
    }

    env = volumeCalc(op + 1, am);       // M2
    if (env < ENV_QUIET) {
        *(op + 1)->connect += opCalc(op + 1, env, m_m2);
    }

    env = volumeCalc(op + 2, am);       // C1
    if (env < ENV_QUIET) {
        *(op + 2)->connect += opCalc(op + 2, env, m_c1);
    }

    env = volumeCalc(op + 3, am);       // C2
    if (env < ENV_QUIET) {
        m_chanOut[chan] += opCalc(op + 3, env, m_c2);
    }

    op->memValue = m_mem;               // M1
}

// Channel 7 is the same as the others except that C2 can be replaced by the
// noise generator.
void Ym2151::chan7Calc() {
    m_m2 = m_c1 = m_c2 = m_mem = 0;

    Operator* op = &m_oper[7 * 4];       // M1
    *op->memConnect = op->memValue;

    u32 am = op->ams ? (m_lfa << (op->ams - 1)) : 0;
    u32 env = volumeCalc(op, am);

    s32 out = op->feedbackOutPrevious + op->feedbackOutCurrent;
    op->feedbackOutPrevious = op->feedbackOutCurrent;

    if (!op->connect) {
        m_mem = m_c1 = m_c2 = op->feedbackOutPrevious;      // algorithm 5
    } else {
        *op->connect = op->feedbackOutPrevious;
    }

    op->feedbackOutCurrent = 0;
    if (env < ENV_QUIET) {
        if (!op->feedbackShift) {
            out = 0;
        }
        op->feedbackOutCurrent = opCalc1(op, env, out << op->feedbackShift);
    }

    env = volumeCalc(op + 1, am);       // M2
    if (env < ENV_QUIET) {
        *(op + 1)->connect += opCalc(op + 1, env, m_m2);
    }

    env = volumeCalc(op + 2, am);       // C1
    if (env < ENV_QUIET) {
        *(op + 2)->connect += opCalc(op + 2, env, m_c1);
    }

    env = volumeCalc(op + 3, am);       // C2
    if (m_noise & 0x80) {
        // The YM2151 noise output ranges from -2044 to 2040; bit 16 of the
        // shift register picks the sign.
        s32 noiseout = (env < 0x3ff) ? static_cast<s32>((env ^ 0x3ff) * 2) : 0;
        m_chanOut[7] += (m_noiseRng & 0x10000) ? noiseout : -noiseout;
    } else if (env < ENV_QUIET) {
        m_chanOut[7] += opCalc(op + 3, env, m_c2);
    }

    op->memValue = m_mem;               // M1
}






// The envelope 'rate' comes from `rate = 2*DR + rks`, where rks is the note
// code after key scaling (0 to 31) and DR is the register value, so the maximum
// is 2*31+31 = 93. The four MSBs are the 'main' rate (00 to 15) and the two
// LSBs select the shape.
//
// One envelope step covers 2048 output samples at main rate 00 and halves with
// every increment, down to one sample at main rate 11; rates 12 to 15 change
// the level on every sample but by larger amounts. A 'sample' here is three
// output samples, because the envelope generator clock is the internal clock
// divided by three.
//
// Two voices running at the same main rate always change level on exactly the
// same samples even if they started at different times, and voices at different
// main rates are delayed by exactly one step per rate increment. Both were
// verified on the real chip.
void Ym2151::advanceEg() {
    m_egTimer += m_egTimerAdd;

    while (m_egTimer >= m_egTimerOverflow) {
        m_egTimer -= m_egTimerOverflow;
        m_egCnt++;

        for (Operator& op : m_oper) {
            // Lets an instrument skip over the attack and decay stages.
            // Fix by Aaron Giles, April 22, 2021.
            if (op.egState == EG_ATT && op.volume <= MIN_ATT_INDEX) {
                op.volume = MIN_ATT_INDEX;
                op.egState = EG_DEC;
            }
            if (op.egState == EG_DEC && static_cast<u32>(op.volume) >= op.sustainLevel) {
                op.egState = EG_SUS;
            }

            switch (op.egState) {
            case EG_ATT:
                if (!(m_egCnt & ((1 << op.egShiftAttack) - 1))) {
                    op.volume += (~op.volume *
                                  egInc[op.egSelectAttack +
                                        ((m_egCnt >> op.egShiftAttack) & 7)]) >> 4;
                    if (op.volume <= MIN_ATT_INDEX) {
                        op.volume = MIN_ATT_INDEX;
                        op.egState = EG_DEC;
                    }
                }
                break;

            case EG_DEC:
                if (!(m_egCnt & ((1 << op.egShiftDecay) - 1))) {
                    op.volume += egInc[op.egSelectDecay +
                                       ((m_egCnt >> op.egShiftDecay) & 7)];
                    if (static_cast<u32>(op.volume) >= op.sustainLevel) {
                        op.egState = EG_SUS;
                    }
                }
                break;

            case EG_SUS:
                if (!(m_egCnt & ((1 << op.egShiftSustain) - 1))) {
                    op.volume += egInc[op.egSelectSustain +
                                       ((m_egCnt >> op.egShiftSustain) & 7)];
                    if (op.volume >= MAX_ATT_INDEX) {
                        op.volume = MAX_ATT_INDEX;
                        op.egState = EG_OFF;
                    }
                }
                break;

            case EG_REL:
                if (!(m_egCnt & ((1 << op.egShiftRelease) - 1))) {
                    op.volume += egInc[op.egSelectRelease +
                                       ((m_egCnt >> op.egShiftRelease) & 7)];
                    if (op.volume >= MAX_ATT_INDEX) {
                        op.volume = MAX_ATT_INDEX;
                        op.egState = EG_OFF;
                    }
                }
                break;
            }
        }
    }
}


void Ym2151::advance() {
    if (m_test & 2) {
        m_lfoPhase = 0;
    } else {
        m_lfoTimer += m_lfoTimerAdd;
        if (m_lfoTimer >= m_lfoOverflow) {
            m_lfoTimer   -= m_lfoOverflow;
            m_lfoCounter += m_lfoCounterAdd;
            m_lfoPhase   += m_lfoCounter >> 4;
            m_lfoPhase   &= 255;
            m_lfoCounter &= 15;
        }
    }

    // LFO AM and PM waveform values, all verified on the real chip except for
    // the noise algorithm, which is impossible to analyse.
    u32 i = m_lfoPhase;
    s32 a = 0;
    s32 p = 0;
    switch (m_lfoWaveform) {
    case 0:     // saw: AM 255 down to 0, PM 0 to 127 then -127 to 0
        a = 255 - static_cast<s32>(i);
        p = (i < 128) ? static_cast<s32>(i) : static_cast<s32>(i) - 255;
        break;

    case 1:     // square: AM 255 or 0, PM exactly +PMD or -PMD
        if (i < 128) {
            a = 255;
            p = 128;
        } else {
            a = 0;
            p = -128;
        }
        break;

    case 2:     // triangle
        // AM: 255 down to 1 step -2, then 0 up to 254 step +2.
        // PM: 0..126 step +2, 127..1 step -2, 0..-126 step -2, -127..-1 step +2.
        a = (i < 128) ? 255 - static_cast<s32>(i) * 2 : static_cast<s32>(i) * 2 - 256;

        if (i < 64) {
            p = static_cast<s32>(i) * 2;
        } else if (i < 128) {
            p = 255 - static_cast<s32>(i) * 2;
        } else if (i < 192) {
            p = 256 - static_cast<s32>(i) * 2;
        } else {
            p = static_cast<s32>(i) * 2 - 511;
        }
        break;

    case 3:
    default:    // random: the real algorithm is unknown, so replay a snapshot
                // taken from the real chip. AM 0..255, PM -128..127.
        a = lfoNoiseWaveform[i];
        p = a - 128;
        break;
    }
    m_lfa = a * static_cast<s32>(m_amd) / 128;
    m_lfp = p * static_cast<s32>(m_pmd) / 128;

    // The noise generator is a 17-bit shift register. Bit 16 is fed the negated
    // (bit0 XOR bit3), ie. EXNOR, and doubles as the noise output.
    m_noisePhase += m_noisePeriod;
    i = m_noisePhase >> 16;     // number of shifts to perform
    m_noisePhase &= 0xffff;
    while (i--) {
        u32 j = (((m_noiseRng ^ (m_noiseRng >> 3)) & 1) ^ 1);
        m_noiseRng = (j << 16) | (m_noiseRng >> 1);
    }

    // Phase generator.
    for (u32 chan = 0; chan < 8; chan++) {
        Operator* op = &m_oper[chan * 4];

        s32 modIndex = 0;
        if (op->pms) {          // only if LFO phase modulation is enabled here
            modIndex = m_lfp;   // -128..+127, 8 bits signed
            modIndex = (op->pms < 6) ? modIndex >> (6 - op->pms) : modIndex << (op->pms - 5);
        }

        if (modIndex) {
            u32 kcChannel = op->kcIndex + modIndex;
            for (u32 j = 0; j < 4; j++) {
                op[j].phase += ((m_freq[kcChannel + op[j].dt2] + op[j].dt1) * op[j].multiple) >> 1;
            }
        } else {
            for (u32 j = 0; j < 4; j++) {
                op[j].phase += op[j].freq;
            }
        }
    }

    // CSM is handled after the phase generator, as verified on the real chip.
    // The CSM key on line appears to be ORed with the KO line inside the chip,
    // so it only has an effect while KO (register 0x08) is zero. With timer A
    // set to 1023 the key on happens every sample and there is never a key off,
    // which sounds identical to a plain key on.
    if (m_csmRequest == 2) {            // key on
        for (Operator& op : m_oper) {
            keyOn(&op, 2);
        }
        m_csmRequest = 1;
    } else if (m_csmRequest) {          // key off
        for (Operator& op : m_oper) {
            keyOff(&op, ~2u);
        }
        m_csmRequest = 0;
    }
}

// Generates `samples` samples into the two mono buffers.
void Ym2151::update(s16* left, s16* right, u32 samples) {
    if (m_timerBEnabled) {
        m_timerBValue -= samples << TIMER_SH;
        if (m_timerBValue <= 0) {
            m_timerBValue += m_timerBTable[m_timerBIndex];
            if (m_irqEnable & 0x08) {
                u32 oldstate = m_status & 3;
                m_status |= 2;
                if (!oldstate && m_irqHandler) {
                    m_irqHandler(true);
                }
            }
        }
    }

    for (u32 i = 0; i < samples; i++) {
        advanceEg();

        for (s32& out : m_chanOut) {
            out = 0;
        }

        for (u32 chan = 0; chan < 7; chan++) {
            chanCalc(chan);
        }
        chan7Calc();

        s32 outl = 0;
        s32 outr = 0;
        for (u32 chan = 0; chan < 8; chan++) {
            outl += m_chanOut[chan] & m_pan[chan * 2 + 0];
            outr += m_chanOut[chan] & m_pan[chan * 2 + 1];
        }

        left[i]  = static_cast<s16>(std::clamp(outl, MINOUT, MAXOUT));
        right[i] = static_cast<s16>(std::clamp(outr, MINOUT, MAXOUT));

        if (m_timerAEnabled) {
            m_timerAValue -= 1 << TIMER_SH;
            if (m_timerAValue <= 0) {
                m_timerAValue += m_timerATable[m_timerAIndex];
                if (m_irqEnable & 0x04) {
                    u32 oldstate = m_status & 3;
                    m_status |= 1;
                    if (!oldstate && m_irqHandler) {
                        m_irqHandler(true);
                    }
                }
                if (m_irqEnable & 0x80) {
                    m_csmRequest = 2;   // request a key on / key off sequence
                }
            }
        }

        advance();
    }
}


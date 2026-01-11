#include "waveform_tables.h"
#include <math.h>

// Calculate at compile time using macros
#define CALC_SINE(i) ((uint8_t)(127.0f + 127.0f * sinf(2.0f * M_PI * (i) / TABLE_SIZE)))
#define CALC_SQUARE(i) ((i) < TABLE_SIZE / 2 ? 255 : 0)
#define CALC_TRIANGLE(i) ((i) < TABLE_SIZE / 2 ? \
    (uint8_t)((2.0f * 255.0f * (i)) / TABLE_SIZE) : \
    (uint8_t)(255.0f - (2.0f * 255.0f * ((i) - TABLE_SIZE / 2)) / TABLE_SIZE))
#define CALC_SAWTOOTH(i) ((uint8_t)((255.0f * (i)) / (TABLE_SIZE - 1)))

// Helper macro to generate array elements
#define GENERATE_LUT(func) { \
    func(0), func(1), func(2), func(3), func(4), func(5), func(6), func(7), \
    func(8), func(9), func(10), func(11), func(12), func(13), func(14), func(15), \
    func(16), func(17), func(18), func(19), func(20), func(21), func(22), func(23), \
    func(24), func(25), func(26), func(27), func(28), func(29), func(30), func(31), \
    func(32), func(33), func(34), func(35), func(36), func(37), func(38), func(39), \
    func(40), func(41), func(42), func(43), func(44), func(45), func(46), func(47), \
    func(48), func(49), func(50), func(51), func(52), func(53), func(54), func(55), \
    func(56), func(57), func(58), func(59), func(60), func(61), func(62), func(63), \
    func(64), func(65), func(66), func(67), func(68), func(69), func(70), func(71), \
    func(72), func(73), func(74), func(75), func(76), func(77), func(78), func(79), \
    func(80), func(81), func(82), func(83), func(84), func(85), func(86), func(87), \
    func(88), func(89), func(90), func(91), func(92), func(93), func(94), func(95), \
    func(96), func(97), func(98), func(99), func(100), func(101), func(102), func(103), \
    func(104), func(105), func(106), func(107), func(108), func(109), func(110), func(111), \
    func(112), func(113), func(114), func(115), func(116), func(117), func(118), func(119), \
    func(120), func(121), func(122), func(123), func(124), func(125), func(126), func(127), \
    func(128), func(129), func(130), func(131), func(132), func(133), func(134), func(135), \
    func(136), func(137), func(138), func(139), func(140), func(141), func(142), func(143), \
    func(144), func(145), func(146), func(147), func(148), func(149), func(150), func(151), \
    func(152), func(153), func(154), func(155), func(156), func(157), func(158), func(159), \
    func(160), func(161), func(162), func(163), func(164), func(165), func(166), func(167), \
    func(168), func(169), func(170), func(171), func(172), func(173), func(174), func(175), \
    func(176), func(177), func(178), func(179), func(180), func(181), func(182), func(183), \
    func(184), func(185), func(186), func(187), func(188), func(189), func(190), func(191), \
    func(192), func(193), func(194), func(195), func(196), func(197), func(198), func(199), \
    func(200), func(201), func(202), func(203), func(204), func(205), func(206), func(207), \
    func(208), func(209), func(210), func(211), func(212), func(213), func(214), func(215), \
    func(216), func(217), func(218), func(219), func(220), func(221), func(222), func(223), \
    func(224), func(225), func(226), func(227), func(228), func(229), func(230), func(231), \
    func(232), func(233), func(234), func(235), func(236), func(237), func(238), func(239), \
    func(240), func(241), func(242), func(243), func(244), func(245), func(246), func(247), \
    func(248), func(249), func(250), func(251), func(252), func(253), func(254), func(255) \
}

// Generate tables at compile time - const for DMA
const uint8_t sine_lut[TABLE_SIZE] = GENERATE_LUT(CALC_SINE);
const uint8_t square_lut[TABLE_SIZE] = GENERATE_LUT(CALC_SQUARE);
const uint8_t triangle_lut[TABLE_SIZE] = GENERATE_LUT(CALC_TRIANGLE);
const uint8_t sawtooth_lut[TABLE_SIZE] = GENERATE_LUT(CALC_SAWTOOTH);

// No initialization needed for compile-time version
void waveform_tables_init(void) {
    // Nothing to do - tables are already initialized at compile time
}
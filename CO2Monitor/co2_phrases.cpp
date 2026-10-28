#include <Arduino.h>
#include "config.h"

// CO2 level phrases — fun messages for the e-ink display

static const char p0_0[] PROGMEM = "Fresh and crisp!";
static const char p0_1[] PROGMEM = "Breathing like a forest.";
static const char p0_2[] PROGMEM = "All clear!";
static const char p0_3[] PROGMEM = "Your brain loves this air.";
static const char p0_4[] PROGMEM = "Perfect oxygen buffet.";

static const char p1_0[] PROGMEM = "Air is getting sleepy...";
static const char p1_1[] PROGMEM = "Time for a tiny breeze.";
static const char p1_2[] PROGMEM = "Could be fresher.";
static const char p1_3[] PROGMEM = "Air is thinking about a window.";
static const char p1_4[] PROGMEM = "Air vibes: meh.";

static const char p2_0[] PROGMEM = "Time to open the window.";
static const char p2_1[] PROGMEM = "Brain would like some O2.";
static const char p2_2[] PROGMEM = "CO2 is plotting something.";
static const char p2_3[] PROGMEM = "Air quality: dormitory level.";
static const char p2_4[] PROGMEM = "Room needs a breather.";

static const char p3_0[] PROGMEM = "Stuffy alert: O2 please.";
static const char p3_1[] PROGMEM = "Low-key suffocating.";
static const char p3_2[] PROGMEM = "Brain performance: -20%.";
static const char p3_3[] PROGMEM = "Open window or forget math.";
static const char p3_4[] PROGMEM = "CO2 is winning.";

static const char p4_0[] PROGMEM = "ALARM: Fresh air NOW.";
static const char p4_1[] PROGMEM = "This isn't air anymore.";
static const char p4_2[] PROGMEM = "Sleepiness mode: unlocked.";
static const char p4_3[] PROGMEM = "CO2 level unacceptable.";
static const char p4_4[] PROGMEM = "Suffer room detected.";

static const char* const CO2_GROUPS[][5] PROGMEM = {
  { p0_0, p0_1, p0_2, p0_3, p0_4 },
  { p1_0, p1_1, p1_2, p1_3, p1_4 },
  { p2_0, p2_1, p2_2, p2_3, p2_4 },
  { p3_0, p3_1, p3_2, p3_3, p3_4 },
  { p4_0, p4_1, p4_2, p4_3, p4_4 },
};

static const char* const CO2_STATUS[] PROGMEM = {
  "Excellent",
  "Good",
  "Moderate",
  "Poor",
  "Very Poor"
};

int co2_index(uint16_t co2) {
  if (co2 < CO2_GOOD) return 0;
  if (co2 < CO2_MODERATE) return 1;
  if (co2 < CO2_POOR) return 2;
  if (co2 < CO2_BAD) return 3;
  return 4;
}

const char* get_co2_phrase(uint16_t co2) {
  int group = co2_index(co2);
  int item = random(5);
  return (const char*)pgm_read_ptr(&CO2_GROUPS[group][item]);
}

const char* get_co2_status(uint16_t co2) {
  int group = co2_index(co2);
  return (const char*)pgm_read_ptr(&CO2_STATUS[group]);
}

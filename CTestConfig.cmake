SET(CTEST_PROJECT_NAME "hawk")
SET(CTEST_NIGHTLY_START_TIME "01:00:00 CEST")

set(CTEST_DROP_SITE_CDASH TRUE)

IF(NOT DEFINED CTEST_DROP_METHOD)
  SET(CTEST_DROP_METHOD "http")
ENDIF(NOT DEFINED CTEST_DROP_METHOD)

IF(CTEST_DROP_METHOD STREQUAL "http")
  SET(CTEST_DROP_SITE "davinci.icm.uu.se")
  SET(CTEST_DROP_LOCATION "/CDash/submit.php?project=hawk")
  SET(CTEST_TRIGGER_SITE "")
ENDIF(CTEST_DROP_METHOD STREQUAL "http")

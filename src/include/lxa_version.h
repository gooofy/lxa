/*
 * lxa version information
 * 
 * This file is the single source of truth for lxa version numbers.
 * Update these values when making releases or significant changes.
 * 
 * Version number format: MAJOR.MINOR.PATCH
 * - MAJOR: Incompatible API changes, major milestones
 * - MINOR: New features, backward-compatible changes  
 * - PATCH: Bug fixes, minor improvements
 */

#ifndef LXA_VERSION_H
#define LXA_VERSION_H

#define LXA_VERSION_MAJOR   0
#define LXA_VERSION_MINOR   4
#define LXA_VERSION_PATCH   0

/* String versions for display */
#define LXA_VERSION_STRING  "0.4.0"

/* Build date - automatically set by compiler */
#define LXA_BUILD_DATE      __DATE__
#define LXA_BUILD_TIME      __TIME__

#endif /* LXA_VERSION_H */

log.use_colors(false)
log.info("Test info message (no color)")
log.warning("Test warning message (no color)")
log.error("Test error message (no color)")

log.use_colors(true)
log.info("Test info message (color)")
log.warning("Test warning message (color)")
log.error("Test error message (color)")

print("Log tests finished")

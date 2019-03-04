DEPFILES=$(wildcard $(addsuffix .d, ${OBJECTFILES} ))
DEPFILES+=$(wildcard $(addsuffix .d, ${P2POBJECTFILES} ))
DEPFILES+=$(wildcard $(addsuffix .d, ${MAINOBJECTFILES} ))
ifneq (${DEPFILES},)
include ${DEPFILES}
endif

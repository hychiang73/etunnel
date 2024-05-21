
obj-$(CPTCFG_YOSHINO_NEW_ETUNNEL) += etunnel.o

etunnel-y := init.o log.o rx.o tx.o trace.o hwsim.o

CFLAGS_trace.o := -I$(src)

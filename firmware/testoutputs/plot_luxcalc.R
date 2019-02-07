# From the 'firmware' dir, run
#
#     make lux_test
#     ./lux_test
#
# prior to running this script.

library("gridExtra")
library("grid")
library("ggplot2")

t <- read.csv("luxcalc.csv")

gain1x = t[t$gain==1,]
gain2x = t[t$gain==2,]
gain4x = t[t$gain==4,]
gain8x = t[t$gain==8,]
gain48x = t[t$gain==48,]
gain96x = t[t$gain==96,]

plot_for_integ <- function (integ_time) {
    d1 = gain1x[gain1x$integ_time==integ_time,]
    d2 = gain2x[gain2x$integ_time==integ_time,]
    d4 = gain4x[gain4x$integ_time==integ_time,]
    d8 = gain8x[gain8x$integ_time==integ_time,]
    d48 = gain48x[gain48x$integ_time==integ_time,]
    d96 = gain96x[gain96x$integ_time==integ_time,]

    p1 <- ggplot(data=d1, aes(x=c0,y=luxf,color=ratio)) + geom_point() + labs(x="c0 at gain=1", y = "lux", color="c1/c0")
    p2 <- ggplot(data=d2, aes(x=c0,y=luxf,color=ratio)) + geom_point() + labs(x="c0 at gain=2", y = "lux", color="c1/c0")
    p4 <- ggplot(data=d4, aes(x=c0,y=luxf,color=ratio)) + geom_point() + labs(x="c0 at gain=4", y = "lux", color="c1/c0")
    p8 <- ggplot(data=d8, aes(x=c0,y=luxf,color=ratio)) + geom_point() + labs(x="c0 at gain=8", y = "lux", color="c1/c0")
    p48 <- ggplot(data=d48, aes(x=c0,y=luxf,color=ratio)) + geom_point() + labs(x="c0 at gain=48", y = "lux", color="c1/c0")
    p96 <- ggplot(data=d96, aes(x=c0,y=luxf,color=ratio)) + geom_point() + labs(x="c0 at gain=96", y = "lux", color="c1/c0")

    grid.arrange(p1, p2, p4, p8, p48, p96, nrow=3)
}
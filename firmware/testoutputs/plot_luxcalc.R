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

p1 <- ggplot(data=gain1x, aes(x=c0,y=luxf,colour=ratio)) + geom_point()
p2 <- ggplot(data=gain2x, aes(x=c0,y=luxf,colour=ratio)) + geom_point()
p4 <- ggplot(data=gain4x, aes(x=c0,y=luxf,colour=ratio)) + geom_point()
p8 <- ggplot(data=gain8x, aes(x=c0,y=luxf,colour=ratio)) + geom_point()
p48 <- ggplot(data=gain48x, aes(x=c0,y=luxf,colour=ratio)) + geom_point()
p96 <- ggplot(data=gain96x, aes(x=c0,y=luxf,colour=ratio)) + geom_point()

graph <- grid.arrange(p1, p2, p4, p8, p48, p96, nrow=3)
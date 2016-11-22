library("Hmisc")
a<-read.table("udp10.txt")[,1]
b<-read.table("tcp10.txt")[,1]
heights = c(mean(a),mean(b))
x <-c(1,2)
dinf<-heights-c(1.96 * sd(a)/sqrt(30), 1.96*sd(b)/sqrt(30))
dsup<-heights+c(1.96 * sd(a)/sqrt(30), 1.96 * sd(b)/sqrt(30))

print (mean(a))
print (mean(b))
print (1.96 * sd(a)/sqrt(30))
print (1.96 * sd(b)/sqrt(30))
png("comp_100km.png")
bp <-barplot(heights, ylim=c(0,1), ylab="PDR (%)",
main="TCP vs UDP em VANETs à 100Km/h",
names=c('UDP', 'TCP'), xlab="Protocolos")
errbar(bp[,1], heights, dsup, dinf, add=T)
dev.off()

# From the 'firmware' dir, run
#
#     make units_test
#     ./units_test
#
# prior to running this script.

library("plotly")
library("reshape2")

t <- read.csv("channels_to_lux.csv")

plot_for_integ_gain <- function (integ, gain) {
    tt <- t[t$gain==gain&t$integ_time==integ,]
    alab <- seq(min(tt$chan1), max(tt$chan1), 512)
    m <- acast(tt, chan1~chan2, value.var="lux_fp")
    plot_ly(z = ~m, x=alab, y=alab, type="surface") %>%
        layout(
            scene = list(
                xaxis=list(title="chan1"),
                yaxis=list(title="chan2"),
                zaxis=list(title="lux")
            )
        ) %>%
        add_surface()
}
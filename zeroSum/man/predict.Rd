% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/predict.R
\name{predict}
\alias{predict}
\title{Description of the predict function for zeroSum s3 objects}
\usage{
predict(fit = NULL, newx = NULL, s = "lambda.min", ...)
}
\arguments{
\item{fit}{fit object generated by zeroSumCVFit() or zeroSumFit()}

\item{newx}{data matrix for which new responses should be predicted (rows==samples)}

\item{s}{determines which lambda should be used for prediction if the
fit is a zeroSumCVFit object: lambda.min or lamda.1SE. If
the fit is a zeroSumFit this argument is ignored.}

\item{...}{other arguments for the normal predict function if the fit
is not a zeroSum object}
}
\value{
predicted values
}
\description{
This function uses zeroSumFit objects or zeroSumCVFit objects
to predict responses for a given data newx.
}
\examples{
set.seed(1)
x <- log2(exampleData$x+1)
y <- exampleData$y
fit <- zeroSumCVFit( x, y, alpha=1)
predict(fit, x, s="lambda.min")

}

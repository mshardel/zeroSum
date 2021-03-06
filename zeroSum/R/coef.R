#' Description of the coef function for zeroSum s3 objects
#'
#' This function returns the coefficients of a zeroSum fit object.
#'
#' @param fit fit object generated by zeroSumCVFit() or zeroSumFit()
#'
#' @param s determines which lambda of a zeroSumCVFit should be returned:
#'          lambda.min or lamda.1SE
#'
#' @param ... other arguments for the normal predict function if the fit
#'            is not a zeroSumFit or zeroSumCVFit object
#'
#' @return estimated coefficients
#'
#' @examples
#' set.seed(1)
#' x <- log2(exampleData$x+1)
#' y <- exampleData$y
#' fit <- zeroSumCVFit( x, y, alpha=1)
#' coef(fit, s="lambda.min")
#'
#' @export
coef <- function( fit=NULL, s="lambda.min", ... )
{
    if( any( class(fit)=="ZeroSumCVFit") || any( class(fit)=="ZeroSumFit") )
    {
        if( s == "lambda.min")
        {
            beta <- fit$coef[[ fit$LambdaMinIndex ]]
        }
        else if( s == "lambda.1SE" || s == "lambda.1se")
        {
            beta <- fit$coef[[ fit$Lambda1SEIndex ]]
        }
        else
        {
            beta <- fit$coef[[s]]
        }
        rownames(beta) <- fit$varNames


        return(beta)

    } else
    {
        UseMethod("coef")
    }
}

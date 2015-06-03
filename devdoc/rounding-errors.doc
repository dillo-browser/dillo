/** \page rounding-errors How to Avoid Rounding Errors

(Probably, this is a standard algorithm, so if someone knows the name,
drop me a note.)

If something like

\f[y_i = {x_i a \over b}\f]

is to be calculated, and all numbers are integers, a naive
implementation would result in something, for which

\f[\sum y_i \ne {(\sum x_i) a \over b}\f]

because of rounding errors, due to the integer division. This can be
avoided by transforming the formula into

\f[y_i = {(\sum_{j=0}^{j=i} x_j) a \over b} - \sum_{j=0}^{j=i-1} y_j\f]

Of corse, when all \f$y_i\f$ are calculated in a sequence,
\f$\sum_{j=0}^{j=i} x_j\f$ and \f$\sum_{j=0}^{j=i-1} y_j\f$ can be
accumulated in the same loop. Regard this as sample:

\code
int n, x[n], a, b; // Should all be initialized.
int y[n], cumX = 0, cumY = 0;

for (int i = 0; i < n; i++) {
   cumX += x[i]
   y[i] = (cumX * a) / b - cumY;
   cumY += y[i];
}
\endcode

*/

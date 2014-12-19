#ifndef VERIFY_H
#define VERIFY_H

#include <QtCore>

#ifndef QT_NO_DEBUG
#  define VERIFY(cond) Q_ASSERT(cond)
#else
#  define VERIFY(cond) ((void)(cond))
#endif

#endif // VERIFY_H

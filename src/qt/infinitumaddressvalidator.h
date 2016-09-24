// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef INFINITUM_QT_INFINITUMADDRESSVALIDATOR_H
#define INFINITUM_QT_INFINITUMADDRESSVALIDATOR_H

#include <QValidator>

/** Base58 entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
class InfinitumAddressEntryValidator : public QValidator
{
    Q_OBJECT

public:
    explicit InfinitumAddressEntryValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

/** Infinitum address widget validator, checks for a valid infinitum address.
 */
class InfinitumAddressCheckValidator : public QValidator
{
    Q_OBJECT

public:
    explicit InfinitumAddressCheckValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

#endif // INFINITUM_QT_INFINITUMADDRESSVALIDATOR_H

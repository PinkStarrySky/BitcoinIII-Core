// Copyright (c) 2011-2020 The BitcoinIII Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOINIII_QT_BITCOINADDRESSVALIDATOR_H
#define BITCOINIII_QT_BITCOINADDRESSVALIDATOR_H

#include <QValidator>

/** Base58 entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
class BitcoinIIIAddressEntryValidator : public QValidator
{
    Q_OBJECT

public:
    explicit BitcoinIIIAddressEntryValidator(QObject *parent);

    State validate(QString &input, int &pos) const override;
};

/** BitcoinIII address widget validator, checks for a valid bitcoinIII address.
 */
class BitcoinIIIAddressCheckValidator : public QValidator
{
    Q_OBJECT

public:
    explicit BitcoinIIIAddressCheckValidator(QObject *parent);

    State validate(QString &input, int &pos) const override;
};

#endif // BITCOINIII_QT_BITCOINADDRESSVALIDATOR_H

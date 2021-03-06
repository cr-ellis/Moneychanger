#ifndef __STABLE_HPP__
#include <core/stable.hpp>
#endif

#include <gui/ui/activity.hpp>
#include <ui_activity.h>

#include <gui/widgets/compose.hpp>
#include <gui/ui/dlgexportedtopass.hpp>
#include <gui/ui/dlgexportedcash.hpp>

#include <gui/widgets/home.hpp>
#include <gui/widgets/dlgchooser.hpp>
#include <gui/widgets/qrwidget.hpp>
#include <gui/ui/getstringdialog.hpp>

#include <core/moneychanger.hpp>
#include <core/handlers/DBHandler.hpp>
#include <core/handlers/contacthandler.hpp>
#include <core/handlers/modelpayments.hpp>
#include <core/handlers/focuser.h>

#include <opentxs/api/Activity.hpp>
#include <opentxs/api/Api.hpp>
#include <opentxs/api/ContactManager.hpp>
#include <opentxs/api/OT.hpp>
#include <opentxs/contact/Contact.hpp>
#include <opentxs/contact/ContactData.hpp>
#include <opentxs/client/OTAPI_Exec.hpp>
#include <opentxs/client/OTME_too.hpp>
#include <opentxs/client/OTRecordList.hpp>
#include <opentxs/core/Log.hpp>
#include <opentxs/core/OTTransaction.hpp>
#include <opentxs/core/OTTransactionType.hpp>
#include <opentxs/Types.hpp>

#include <QLabel>
#include <QDebug>
#include <QToolButton>
#include <QKeyEvent>
#include <QApplication>
#include <QMessageBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMenu>
#include <QList>
#include <QSqlRecord>
#include <QTimer>
#include <QFrame>

#include <string>
#include <map>
#include <tuple>



/*
 * The tree items will have certain values attached to them.
 * I will have to figure out what those are.
 * This is the place in the code where I will put those notes until
 * I have it square.
 * Right now I see:
 *
 * Totals
 *  USD         0.00
 * Hosted Accounts
 *  (green) localhost $0.00
 * Contacts
 *  au (desktop) (local wallet)
 *  Bob (local wallet)
 *  Hirosan3000
 *  Issuer (local wallet)
 *
 * I know there are also Stash Node Pros, if one is paired.
 * I also know that if there are multiple accounts on a single server,
 * then the picture might change. Sometimes it combines them, sometimes it
 * separates them.
 * Next thing I'll do is add a "Bitcoin" currency contract and see how the
 * picture changes...
 *
 * Totals
 *  BTC         0.00000000
 *  USD         0.00
 * Hosted Accounts
 *  (green) localhost
 *    Bobs bitcoin      BTC 0.00000000
 *    Bobs dollars      $0.00
 *    New Issuer Acct   -$10,000,000.00
 *    New Issuer Acct   BTC 0.00000000
 *    Dollars (au)      $10,000,000.00
 * Contacts
 *  au (desktop) (local wallet)
 *  Bob (local wallet)
 *  Hirosan3000
 *  Issuer (local wallet)
 *
 * Okay so I definitely have to be able to filter by ASSET TYPE (if click on one of
 * the totals at the top).
 *
 * I also have to be able to filter by NOTARY (if clicking on "localhost" for example)
 * and in fact clicking on localhost DOES work already! But if I click on the accounts
 * inside the localhost notary, it doesn't seem to filter them yet.
 *
 * So I need to be able to filter by ACCOUNT as well.
 *
 * And currently I can also filter by CONTACT, though I think it's still using the old
 * Moneychanger contacts, not the new opentxs contacts (yet... that's what I'm working
 * on now...)
 * ------------------
 *
 * Let's also contrast this with the left sidebar on the old PAYMENTS dialog. That is an
 * obvious tree (expandable, with triangles) with "All Payments (Unfiltered)" at the
 * top, followed by the (old-style) contacts.
 * Each contact expands to show his Nyms, with a line for each Notary the Nym is known
 * to be registered on. This is all queried from the old Moneychanger contact system.
 *
 * I should also note, there may be a line under a contact that's NOT related to a notary.
 * For example, there may be 2 lines for 2 different Bitmessage addresses associated with
 * that contact.
 * ------------------
 *
 * I say all this because on this new ACTIVITY dialog, we don't have that at all, so anywhere
 * that's referencing "method types" under Contacts, is not needed in this code. In fact, an
 * Opentxs Notary would just be another "method", but with method type "opentxs" instead of
 * "bitmsg". On the payments dialog, it's a branch under the Contact on the tree. Whereas in
 * this new Activity dialog, it's just coincidental that we also have a need to filter based
 * on the Notary.
 * And in this case, it really is PURELY about the Notary, regardless of the asset type or
 * the Nym, or the Contact. Just purely the Notary alone (if that's what the user clicked
 * on).
 * But the user can also click on an account under that Notary -- in which case it's PURELY about
 * the ACCOUNT! That is, the Notary is still nice and all, since it's the notary of the account
 * we're filtering for, but in that case it's just circumstantial.
 * ------------------
 *
 * Another thing we know is that, in addition to the "Hosted Accounts" and the "Stash Node Pro",
 * there is also the "Local Wallets", even though they do not currently exist in Moneychanger.
 * (But they do exist on Android). That's still important to mention since local wallets have
 * the property that their accounts are NOT Opentxs accounts. The account is directly a Bitcoin
 * account in a local Bitcoin wallet on the local device, for example, rather than a Bitcoin
 * wallet on a StashNodePro somewhere on the Internet, being represented by way of an Opentxs balance.
 *
 * In anticipation of this, we cannot assume there to be "opentxs" type for asset types, nor
 * for "Local Wallet" accounts (though we do for "Hosted Accounts"). Nor for Contacts, who
 * may only have a Bitmessage address or Bitcoin address, and not any Opentxs accounts at all!
 * This means, in my mind, an Opentxs Notary filter must be kept blank, EXCEPT when the user has
 * directly clicked on an Opentxs Notary!
 * ------------------
 * Filter types:
 *
 * So far here's what I'm thinking. There's a HEADER type which will just have no user data.
 * (Like the headers for Totals, Hosted Accounts, Local Wallets, Stash Node Pros, and Contacts).
 *
 * There's also an ASSET type which has to transcend Opentxs asset type. Because the BTC balance
 * in the local Bitcoin wallet doesn't have an Opentxs asset type ID. But the BTC balance in the
 * Stash Node Pro DOES have an Opentxs asset type ID. And in fact the BTC balance on another
 * Stash Node Pro wll hace ANOTHER Opentxs asset type ID. So I need to do a query that returns
 * ALL the BTC "balances" I have, even the ones with no Opentxs asset type or account ID, and
 * even multiple different Opentxs asset type IDs and account IDs for what is supposedly the
 * same asset type (Bitcoin, in this example).
 *
 * I believe I'm doing that query already to a CERTAIN degree since I am already displaying
 * the account tree on the screen in this dialog. However, I think I'm just adding up the balances,
 * I'm not necessarily actually compiling a list of all the asset types for each asset type
 * (so to speak). I'll check on that.
 *
 * On a top-level "asset type" filter, I don't have or need a Notary ID, or asset type ID, or
 * account ID, though I may have lists of those things.
 *
 * On a "notary" filter, I only need the notary ID and I only want to filter on that alone.
 *
 * On an "account" filter, I only NEED the account ID but I might as well include other information
 * on the account's tree node such as its notary ID or asset type or balance. Why? I dunno, in case
 * it's useful? But I don't think I really need it at all...
 * But if I do keep asset type, I will need the "'BTC' versus 'LTC'" aspect of it, as well as the
 * Opentxs "asset type ID" aspect of it (IF applicable).
 *
 * I'm going to test creating a second Currency with the same BTC currency symbol... yup it does it by symbol. (TLA).
 *
 * Another note: The Hosted accounts are a loop through the servers, and accounts for each server. (Opentxs).
 * But the Stash Node Pros are looping through OTME_too's paired SNPs, not all of which even HAVE a server
 * ID yet! So we can't ASSUME we have a server ID, even in cases where we know we eventually WILL have one!
 *
 *
#define ACTIVITY_TREE_HEADER_TOTALS   0
#define ACTIVITY_TREE_HEADER_LOCAL    1
#define ACTIVITY_TREE_HEADER_SNP      2
#define ACTIVITY_TREE_HEADER_HOSTED   3
#define ACTIVITY_TREE_HEADER_CONTACTS 4

#define AC_NODE_TYPE_HEADER          0
#define AC_NODE_TYPE_ASSET_TOTAL     1
#define AC_NODE_TYPE_CONTACT         2
#define AC_NODE_TYPE_LOCAL_WALLET    3
#define AC_NODE_TYPE_LOCAL_ACCOUNT   4
#define AC_NODE_TYPE_SNP_NOTARY      5
#define AC_NODE_TYPE_SNP_ACCOUNT     6
#define AC_NODE_TYPE_HOSTED_NOTARY   7
#define AC_NODE_TYPE_HOSTED_ACCOUNT  8
 *
 * So UserData(0) should be the NODE TYPE. For the first row, it will be a header (Totals)
 * and so the "node type" should be: AC_NODE_TYPE_HEADER.
 * For the second row, it will be an asset total (probably BTC or USD) so the "node type"
 * would be: AC_NODE_TYPE_ASSET_TOTAL. Again, set in UserData(0).
 *
 * EVERY row in the tree is also under a "header", and so should have the header set.
 * Continuing the above example, the first row has "header" of ACTIVITY_TREE_HEADER_TOTALS
 * and the second row also has "header" of ACTIVITY_TREE_HEADER_TOTALS. This should probably
 * be UserData(1).
 *
 * IF the node type is AC_NODE_TYPE_ASSET_TOTAL, as seen just above for the second row,
 * we probably also want to have the TLA available. This should probably be UserData(2).
 *
 * IF the node type is instead, say, AC_NODE_TYPE_SNP_ACCOUNT or AC_NODE_TYPE_HOSTED_ACCOUNT,
 * tracking account instead of asset ID, then in that case UserData(2) should contain the TLA
 * as before (for consistency's sake). Therefore the account ID should be in UserData(4).
 *
 * That way I can put the Opentxs asset type ID (if one exists) into UserData(3).
 *
 * Let's say instead the "node type" is AC_NODE_TYPE_HOSTED_NOTARY -- in that case, the "header"
 * is ACTIVITY_TREE_HEADER_HOSTED. So that's user data 0 and 1.
 * Then UserData(2) would contain the Opentxs Notary ID, if one was available. (Which is possible,
 * at least in the case of AC_NODE_TYPE_SNP_NOTARY).
 * NO -- this should also contain the TLA in 2, the asset type in 3, and the account ID in 4,
 * IF APPLICABLE, since the notary MAY display a "rolled up" balance total as well, and I prefer
 * to have those values available if they are relevant, in a way consistent with the above.
 * (However, once again, I cannot assume they are present. They may not be.)
 *
 * Therefore the Notary ID must be in UserData(5)! IF it's present at all.
 *
 * Therefore if it turns out to be a Contact, "node type" AC_NODE_TYPE_CONTACT, the Contact ID
 * itself, a new-style string-based Opentxs Contact ID, will preferably be in UserData(6).
 */

//QString qstrCurrentTLA_;  // If the user clicks on "BTC" near the top, the current TLA is "BTC".
//QString qstrCurrentNotary_; // If the user clicks on "localhost" (hosted notary) then that Notary ID is set here.
//QString qstrCurrentAccount_; // If the user clicks on one of his Opentxs accounts, or a notary with only 1 account under it, we put the acct ID here.
//QString qstrCurrentContact_; // If the user clicks on one of his contacts, we put the Opentxs Contact ID here.

ACTIVITY_TREE_ITEM Activity::make_tree_item( QString & qstrCurrentTLA, QString & qstrCurrentNotary, QString & qstrCurrentAccount, QString & qstrCurrentContact)
{
    return std::make_tuple(qstrCurrentTLA.toStdString(), qstrCurrentNotary.toStdString(), qstrCurrentAccount.toStdString(), qstrCurrentContact.toStdString());
}


void Activity::set_inbox_pmntid_for_tree_item(ACTIVITY_TREE_ITEM & theItem, int nPmntID)
{
    mapOfActivityTreeItems::iterator it = mapCurrentRows_inbox.find(theItem);
    if (mapCurrentRows_inbox.end() != it) // found it.
    {
        mapCurrentRows_inbox.erase(it);
    }
    mapCurrentRows_inbox.insert( std::pair<ACTIVITY_TREE_ITEM, int> (theItem, nPmntID) );
}

void Activity::set_outbox_pmntid_for_tree_item(ACTIVITY_TREE_ITEM & theItem, int nPmntID)
{
    mapOfActivityTreeItems::iterator it = mapCurrentRows_outbox.find(theItem);
    if (mapCurrentRows_outbox.end() != it) // found it.
    {
        mapCurrentRows_outbox.erase(it);
    }
    mapCurrentRows_outbox.insert( std::pair<ACTIVITY_TREE_ITEM, int> (theItem, nPmntID) );
}

int Activity::get_inbox_pmntid_for_tree_item(ACTIVITY_TREE_ITEM & theItem)
{
    mapOfActivityTreeItems::iterator it = mapCurrentRows_inbox.find(theItem);
    if (mapCurrentRows_inbox.end() != it) // found it.
    {
        return it->second;
    }
    return 0;
}

int Activity::get_outbox_pmntid_for_tree_item(ACTIVITY_TREE_ITEM & theItem)
{
    mapOfActivityTreeItems::iterator it = mapCurrentRows_outbox.find(theItem);
    if (mapCurrentRows_outbox.end() != it) // found it.
    {
        return it->second;
    }
    return 0;
}


void Activity::RefreshUserBar()
{
    const std::string str_my_nym_id = Moneychanger::It()->get_default_nym_id().toStdString();
    // ----------------------------------------
    MTNameLookupQT theLookup;
    const QString qstrNymName = QString::fromStdString(theLookup.GetNymName(str_my_nym_id, ""));
    ui->toolButtonMyIdentity->setText(qstrNymName);
    ui->toolButtonMyIdentity2->setText(qstrNymName);
}


void Activity::RefreshAll()
{
    RefreshUserBar();
    RefreshAccountTab();
    RefreshConversationsTab();
}


void Activity::RefreshAccountTab()
{
    RefreshAccountTree();
}

void Activity::ClearAccountTree()
{
    ui->treeWidgetAccounts->clear();
}

//typedef QMap<QString, mapIDName> mapOfMapIDName; // TLA or share symbol, map of IDs/display names.

mapIDName & Activity::GetOrCreateAssetIdMapByCurrencyCode(QString qstrTLA, mapOfMapIDName & bigMap)
{
    mapOfMapIDName::iterator i = bigMap.find(qstrTLA);
    while (i != bigMap.end() && i.key() == qstrTLA) {
        return i.value();
    }
    // else create it.
    bigMap[qstrTLA] = mapIDName{};
    i = bigMap.find(qstrTLA);
    return i.value();
}

void Activity::GetAssetIdMapsByCurrencyCode(mapOfMapIDName & bigMap)
{
    int32_t  asset_count = opentxs::OT::App().API().Exec().GetAssetTypeCount();
    for (int32_t ii = 0; ii < asset_count; ii++)
    {
        const QString OT_id   = QString::fromStdString(opentxs::OT::App().API().Exec().GetAssetType_ID(ii));
        const QString OT_name = QString::fromStdString(opentxs::OT::App().API().Exec().GetAssetType_Name(OT_id.toStdString()));
        const QString qstrTLA = QString::fromStdString(opentxs::OT::App().API().Exec().GetCurrencyTLA(OT_id.toStdString()));
        mapIDName & mapTLA = GetOrCreateAssetIdMapByCurrencyCode(qstrTLA, bigMap);
        mapTLA.insert(OT_id, OT_name);
    }
}

int64_t Activity::GetAccountBalancesTotaledForUnitTypes(const mapIDName & mapUnitTypeIds)
{
    int64_t lTotal = 0;

    mapIDName::const_iterator ci = mapUnitTypeIds.begin();
    while (ci != mapUnitTypeIds.end())
    {
        QString qstrUnitTypeId = ci.key();
        // ----------------------------------------------------
        const int nAccountCount = opentxs::OT::App().API().Exec().GetAccountCount();
        for (int ii = 0; ii < nAccountCount; ++ii)
        {
            std::string accountID   = opentxs::OT::App().API().Exec().GetAccountWallet_ID(ii);
            std::string assetTypeID = opentxs::OT::App().API().Exec().GetAccountWallet_InstrumentDefinitionID(accountID);

            QString qstrAssetTypeId = QString::fromStdString(assetTypeID);
            if (0 == qstrAssetTypeId.compare(qstrUnitTypeId))
            {
                const int64_t lSubtotal = opentxs::OT::App().API().Exec().GetAccountWallet_Balance(accountID);

                // We don't count the negative balances, since those are for issuer accounts,
                // and if we included them, the total would always balance out to zero anyway.
                if (lSubtotal > 0)
                {
                    lTotal += lSubtotal;
                }
            }
        }
        // ----------------------------------------------------
        ++ci;
    }
    return lTotal;
}


void Activity::GetAccountIdMapsByServerId(mapOfMapIDName & bigMap, bool bPairedOrHosted) // true == paired, false == hosted.
{
    int32_t  account_count = opentxs::OT::App().API().Exec().GetAccountCount();

    for (int32_t ii = 0; ii < account_count; ii++)
    {
        const QString OT_id   = QString::fromStdString(opentxs::OT::App().API().Exec().GetAccountWallet_ID(ii));
        const QString OT_name = QString::fromStdString(opentxs::OT::App().API().Exec().GetAccountWallet_Name(OT_id.toStdString()));
        const QString qstrServerId = QString::fromStdString(opentxs::OT::App().API().Exec().GetAccountWallet_NotaryID(OT_id.toStdString()));
        const std::string str_server_id = qstrServerId.toStdString();

        // bPaireding means, paired or in the process of pairing.
        const bool bPaireding = opentxs::OT::App().API().OTME_TOO().PairingStarted(str_server_id);

        // Basically the caller either wants a list of accounts by server ID for PAIRED servers,
        // OR he wants a list of accounts by server ID for NON-Paired (hosted) servers.
        // So those are the two cases where we add the server to the list.
        if ( (bPairedOrHosted && bPaireding) || (!bPairedOrHosted && !bPaireding))
        {
            mapIDName & mapServerId = GetOrCreateAccountIdMapByServerId(qstrServerId, bigMap);
            mapServerId.insert(OT_id, OT_name);
        }
    }
}

mapIDName & Activity::GetOrCreateAccountIdMapByServerId(QString qstrServerId, mapOfMapIDName & bigMap)
{
    mapOfMapIDName::iterator i = bigMap.find(qstrServerId);
    while (i != bigMap.end() && i.key() == qstrServerId) {
        return i.value();
    }
    // else create it.
    bigMap[qstrServerId] = mapIDName{};
    i = bigMap.find(qstrServerId);
    return i.value();
}


/*
#define ACTIVITY_TREE_HEADER_TOTALS   0
#define ACTIVITY_TREE_HEADER_LOCAL    1
#define ACTIVITY_TREE_HEADER_SNP      2
#define ACTIVITY_TREE_HEADER_HOSTED   3
#define ACTIVITY_TREE_HEADER_CONTACTS 4

#define AC_NODE_TYPE_HEADER          0
#define AC_NODE_TYPE_ASSET_TOTAL     1
#define AC_NODE_TYPE_CONTACT         2
#define AC_NODE_TYPE_LOCAL_WALLET    3
#define AC_NODE_TYPE_LOCAL_ACCOUNT   4
#define AC_NODE_TYPE_SNP_NOTARY      5
#define AC_NODE_TYPE_SNP_ACCOUNT     6
#define AC_NODE_TYPE_HOSTED_NOTARY   7
#define AC_NODE_TYPE_HOSTED_ACCOUNT  8
 *
 * So UserData(0) should be the NODE TYPE. For the first row, it will be a header (Totals)
 * and so the "node type" should be: AC_NODE_TYPE_HEADER and "header" will be ACTIVITY_TREE_HEADER_TOTALS.
 *
 * For the second row, it will be an asset total (probably BTC or USD) so the "node type"
 * would be: AC_NODE_TYPE_ASSET_TOTAL. Again, set in UserData(0).
 *
 * EVERY row in the tree is also under a "header", and so should have the header set.
 * Continuing the above example, the first row has "header" of ACTIVITY_TREE_HEADER_TOTALS
 * and the second row also has "header" of ACTIVITY_TREE_HEADER_TOTALS. This should probably
 * be UserData(1).
 *
 * IF the node type is AC_NODE_TYPE_ASSET_TOTAL, as seen just above for the second row,
 * we probably also want to have the TLA available. This should probably be UserData(2).
 *
 * IF the node type is instead, say, AC_NODE_TYPE_SNP_ACCOUNT or AC_NODE_TYPE_HOSTED_ACCOUNT,
 * tracking account instead of asset ID, then in that case UserData(2) should contain the TLA
 * as before (for consistency's sake). Therefore the account ID should be in UserData(4).
 *
 * That way I can put the Opentxs asset type ID (if one exists) into UserData(3).
 *
 * Let's say instead the "node type" is AC_NODE_TYPE_HOSTED_NOTARY -- in that case, the "header"
 * is ACTIVITY_TREE_HEADER_HOSTED. So that's user data 0 and 1.
 * Then UserData(2) would contain the Opentxs Notary ID, if one was available. (Which is possible,
 * at least in the case of AC_NODE_TYPE_SNP_NOTARY).
 * NO -- this should also contain the TLA in 2, the asset type in 3, and the account ID in 4,
 * IF APPLICABLE, since the notary MAY display a "rolled up" balance total as well, and I prefer
 * to have those values available if they are relevant, in a way consistent with the above.
 * (However, once again, I cannot assume they are present. They may not be.)
 *
 * Therefore the Notary ID must be in UserData(5)! IF it's present at all.
 *
 * Therefore if it turns out to be a Contact, "node type" AC_NODE_TYPE_CONTACT, the Contact ID
 * itself, a new-style string-based Opentxs Contact ID, will preferably be in UserData(6).
 */

//void blah(QString strID) // asset ID
//{
//    // ----------------------------------
//    const QString qstrContents = QString::fromStdString(opentxs::OT::App().API().Exec().GetAssetType_Contract(strID.toStdString()));
//    opentxs::proto::UnitDefinition contractProto = opentxs::proto::StringToProto<opentxs::proto::UnitDefinition>
//                                                    (opentxs::String(qstrContents.toStdString()));
//    // ----------------------------------
//    QString qstrDetails("");

//    qstrDetails += contractProto.has_name() ? QString("- %1: %2\n").arg(tr("Name")).arg(QString::fromStdString(contractProto.name())) : QString("");
//    qstrDetails += contractProto.has_version() ? QString("- %1: %2\n").arg(tr("Version")).arg(QString::number(contractProto.version())) : QString("");
//    qstrDetails += contractProto.has_shortname() ? QString("- %1: %2\n").arg(tr("Short Name")).arg(QString::fromStdString(contractProto.shortname())) : QString("");
//    qstrDetails += contractProto.has_symbol() ? QString("- %1: %2\n").arg(tr("Symbol")).arg(QString::fromStdString(contractProto.symbol())) : QString("");

//    if (contractProto.has_currency()) {
//        qstrDetails += QString("%1:\n").arg(tr("Is currency"));
//        auto & params = contractProto.currency();
//        qstrDetails += params.has_tla() ? QString("- %1: %2\n").arg(tr("TLA")).arg(QString::fromStdString(params.tla())) : QString("");
//        qstrDetails += params.has_version() ? QString("- %1: %2\n").arg(tr("Version")).arg(QString::number(params.version())) : QString("");
//        qstrDetails += params.has_fraction() ? QString("- %1: %2\n").arg(tr("Fraction")).arg(QString::fromStdString(params.fraction())) : QString("");
//        qstrDetails += params.has_power() ? QString("- %1: %2\n").arg(tr("Power")).arg(QString::number(params.power())) : QString("");
//    }
//    if (contractProto.has_security()) {
//        qstrDetails += QString("%1:\n").arg(tr("Is shares"));
//        auto & params = contractProto.security();
//        qstrDetails += params.has_version() ? QString("- %1: %2\n").arg(tr("Version")).arg(QString::number(params.version())) : QString("");
//    }
//    if (contractProto.has_basket()) {
//        qstrDetails += QString("%1:\n").arg(tr("Is basket"));
//        auto & params = contractProto.basket();
//        qstrDetails += params.has_version() ? QString("- %1: %2\n").arg(tr("Version")).arg(QString::number(params.version())) : QString("");
//    }

//    qstrDetails += contractProto.has_terms() ? QString("- %1:\n%2").arg(tr("Terms")).arg(QString::fromStdString(contractProto.terms())) : QString("");

//}

void Activity::RefreshAccountTree()
{
    // ----------------------------------------
    ClearAccountTree();
    // ----------------------------------------
    QTreeWidget * pTreeWidgetAccounts = ui->treeWidgetAccounts;
    if (nullptr == pTreeWidgetAccounts) {
        return;
    }
    pTreeWidgetAccounts->blockSignals(true);
    // ----------------------------------------
    QList<QTreeWidgetItem *> items;
    // ------------------------------------
    mapOfMapIDName bigMap;

    this->GetAssetIdMapsByCurrencyCode(bigMap);

    if (bigMap.size() > 0)
    {
        // NOTE: Someday this "Totals" row is where we will someday replace with the USD value of the total wallet.
        QTreeWidgetItem * pUnfilteredItem = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(tr("Totals")));
        items.append(pUnfilteredItem);
        pUnfilteredItem->setExpanded(true);
        pUnfilteredItem->setData(0, Qt::UserRole  , QVariant(AC_NODE_TYPE_HEADER));
        pUnfilteredItem->setData(0, Qt::UserRole+1, QVariant(ACTIVITY_TREE_HEADER_TOTALS));
        pUnfilteredItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
        pUnfilteredItem->setFlags(pUnfilteredItem->flags()&~Qt::ItemIsSelectable);
        pUnfilteredItem->setTextColor(0, Qt::white);
        pUnfilteredItem->setTextColor(1, Qt::white);
        pUnfilteredItem->setBackgroundColor(0, Qt::gray);
        pUnfilteredItem->setBackgroundColor(1, Qt::gray);
        pUnfilteredItem->setBackgroundColor(2, Qt::gray);
        pUnfilteredItem->setBackgroundColor(3, Qt::gray);
        // ----------------------------------------
        // Get the total balance for each list of unit types.
        // (That is, the total balance for each currency code).
        mapOfMapIDName::const_iterator ci = bigMap.begin();

        while (ci != bigMap.end()) {
            const QString qstrCurrencyCode = ci.key();
            const mapIDName & mapTLA = ci.value();
            const int64_t nBalanceTotal = GetAccountBalancesTotaledForUnitTypes(mapTLA);
            const QString qstrFirstAssetTypeId = mapTLA.begin().key();
            const std::string first_unit_type  = qstrFirstAssetTypeId.toStdString();
            const std::string str_formatted_amount = opentxs::OT::App().API().Exec().FormatAmountWithoutSymbol(first_unit_type, nBalanceTotal);
            const QString qstrFormattedAmount = QString::fromStdString(str_formatted_amount);

            const std::string asset_contract = opentxs::OT::App().API().Exec().GetAssetType_Contract(first_unit_type);
            opentxs::proto::UnitDefinition contractProto = opentxs::proto::StringToProto<opentxs::proto::UnitDefinition>
                                                            (opentxs::String(asset_contract));
            const std::string str_shortname = contractProto.has_shortname() ? contractProto.shortname() : "";
            const std::string str_name = contractProto.has_name() ? contractProto.name() : "";
            const QString qstrName(QString::fromStdString(str_name));
            const std::string str_symbol = (contractProto.has_currency() && contractProto.has_symbol()) ? contractProto.symbol() : "";
            const QString qstrSymbol(QString::fromStdString(str_symbol));

            QTreeWidgetItem * pCurrencyCodeWidgetItem = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(qstrName) << qstrSymbol<< qstrFormattedAmount << QStringList(qstrCurrencyCode));
            items.append(pCurrencyCodeWidgetItem);
            pCurrencyCodeWidgetItem->setExpanded(false);
            pCurrencyCodeWidgetItem->setData(0, Qt::UserRole  , QVariant(AC_NODE_TYPE_ASSET_TOTAL));
            pCurrencyCodeWidgetItem->setData(0, Qt::UserRole+1, QVariant(ACTIVITY_TREE_HEADER_TOTALS));
            pCurrencyCodeWidgetItem->setData(0, Qt::UserRole+2, QVariant(qstrCurrencyCode));

            pCurrencyCodeWidgetItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);

            ++ci;
        }

//        QTreeWidgetItem * pDefaultCurrency = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList("USD") << "3,487.32");
//        items.append(pDefaultCurrency);
//        pDefaultCurrency->setExpanded(false);
//        pDefaultCurrency->setData(0, Qt::UserRole, QVariant(0));
//        pDefaultCurrency->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
    }

    // ----------------------------------------
    mapOfMapIDName bigMapPairedAccounts;

    const uint64_t paired_node_count = opentxs::OT::App().API().OTME_TOO().PairedNodeCount();

    if (paired_node_count > 0)
    {
        this->GetAccountIdMapsByServerId(bigMapPairedAccounts, true); // True means, only give me paired servers.
        // ----------------------------------------
        QTreeWidgetItem * pYourNodeItem = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(tr("Your Stash Node")));
        items.append(pYourNodeItem);
        pYourNodeItem->setExpanded(true);
        pYourNodeItem->setData(0, Qt::UserRole  , QVariant(AC_NODE_TYPE_HEADER));
        pYourNodeItem->setData(0, Qt::UserRole+1, QVariant(ACTIVITY_TREE_HEADER_SNP));
        pYourNodeItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
        pYourNodeItem->setTextColor(0, Qt::white);
        pYourNodeItem->setTextColor(1, Qt::white);
        pYourNodeItem->setBackgroundColor(0, Qt::gray);
        pYourNodeItem->setBackgroundColor(1, Qt::gray);
        pYourNodeItem->setBackgroundColor(2, Qt::gray);
        pYourNodeItem->setBackgroundColor(3, Qt::gray);
        pYourNodeItem->setFlags(pYourNodeItem->flags()&~Qt::ItemIsSelectable);
    }

    for (int index = 0; index < paired_node_count; index++) // FOR EACH PAIRED OR PAIRING NODE.
    {
        QString qstrSnpName = tr("Pairing Stash Node...");
        QString qstrServerId;
        const std::string str_index = QString::number(index).toStdString();

        // bPaireding means, paired or in the process of pairing.
        const bool bPaireding = opentxs::OT::App().API().OTME_TOO().PairingStarted(str_index);
        bool bConnected = false;
        // ------------------------------------------------------------------------
        if (bPaireding)  // If paired or pairing.
        {
            const std::string str_snp_server_id = opentxs::OT::App().API().OTME_TOO().GetPairedServer(str_index);
            const bool bGotNotaryId = str_snp_server_id.size() > 0;

            if (bGotNotaryId)
            {
                qstrServerId = QString::fromStdString(str_snp_server_id);

                bConnected = opentxs::OT::App().API().Exec().CheckConnection(str_snp_server_id);
                QString strConnected = QString(bConnected ? "Connected" : "Not connected");

                //String strLog = "SNP Notary ID: " + str_snp_server_id + " - " + strConnected;
                //Log.d("MService", strLog);

                QString strTemp = QString::fromStdString(opentxs::OT::App().API().Exec().GetServer_Name(str_snp_server_id));
                if (strTemp.size() > 0)
                {
                    qstrSnpName = strTemp;
                }
                else
                {
                    qDebug() << "Failed to get notary name for this notary ID: " << QString::fromStdString(str_snp_server_id);
                }
            }
            else
            {
                qDebug() << "Failed to get notary ID for this index: " << QString::fromStdString(str_index);
            }
        }
        else
        {
            qDebug() << "This index is NOT paired or pairing yet: str_index: " << QString::fromStdString(str_index);
        }
        // ------------------------------------------------------------------------
        if (qstrServerId.isEmpty())
        {
            QTreeWidgetItem * pNodeItem = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(qstrSnpName) );
            items.append(pNodeItem);
            pNodeItem->setExpanded(false);
            const QString qstrIconPath(bConnected ? ":/icons/icons/green_dot.png" : ":/icons/icons/red_dot.png");
            pNodeItem->setIcon(0, QIcon(qstrIconPath));
            pNodeItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);

            pNodeItem->setData(0, Qt::UserRole  , QVariant(AC_NODE_TYPE_SNP_NOTARY));
            pNodeItem->setData(0, Qt::UserRole+1, QVariant(ACTIVITY_TREE_HEADER_SNP));

        }
        else // We know for this paired node, there's a server Id.
        {
            // So let's see if there are any accounts for that server Id.
            mapOfMapIDName::const_iterator ci_paired = bigMapPairedAccounts.find(qstrServerId);

            if (ci_paired != bigMapPairedAccounts.end() && (0 == qstrServerId.compare(ci_paired.key()))) // Found some.
            {
                const mapIDName & mapAccounts = ci_paired.value();
                const std::string str_server_id = qstrServerId.toStdString();
                qstrSnpName = QString::fromStdString(opentxs::OT::App().API().Exec().GetServer_Name(str_server_id));

                if (1 == mapAccounts.size())
                {
                    const QString qstrAccountId = mapAccounts.begin().key();
                    const std::string str_acct_id = qstrAccountId.toStdString();
                    const std::string str_asset_id = opentxs::OT::App().API().Exec().GetAccountWallet_InstrumentDefinitionID(str_acct_id);
                    const int64_t lBalance = opentxs::OT::App().API().Exec().GetAccountWallet_Balance(str_acct_id);
                    const std::string str_formatted_amount = opentxs::OT::App().API().Exec().FormatAmountWithoutSymbol(str_asset_id, lBalance);
//                  const std::string str_formatted_amount = opentxs::OT::App().API().Exec().FormatAmount(str_asset_id, lBalance);

                    const std::string asset_contract = opentxs::OT::App().API().Exec().GetAssetType_Contract(str_asset_id);
                    opentxs::proto::UnitDefinition contractProto = opentxs::proto::StringToProto<opentxs::proto::UnitDefinition>
                                                                    (opentxs::String(asset_contract));

                    const std::string str_symbol = (contractProto.has_currency() && contractProto.has_symbol()) ? contractProto.symbol() : "";
                    const QString qstrSymbol(QString::fromStdString(str_symbol));
                    const QString qstrFormattedAmount = QString::fromStdString(str_formatted_amount);
                    const QString qstrAssetTypeId = QString::fromStdString(str_asset_id);
                    const QString qstrTLA = QString::fromStdString(opentxs::OT::App().API().Exec().GetCurrencyTLA(str_asset_id));
                    // ------------------
                    QTreeWidgetItem * pPairedItem = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(qstrSnpName) << qstrSymbol << qstrFormattedAmount << qstrTLA);
                    items.append(pPairedItem);
                    pPairedItem->setExpanded(false);

                    const QString qstrIconPath = QString(bConnected ? ":/icons/icons/green_dot.png" : ":/icons/icons/red_dot.png");
                    pPairedItem->setIcon(0, QIcon(qstrIconPath));
                    if (bConnected)
                    {
    //                    pPairedItem->setTextColor(0, Qt::black);
    //                    pPairedItem->setTextColor(1, Qt::black);
                    }
                    else
                    {
                        pPairedItem->setTextColor(0, Qt::gray);
                        pPairedItem->setTextColor(1, Qt::gray);
                    }
                    pPairedItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);

                    pPairedItem->setData(0, Qt::UserRole  , QVariant(AC_NODE_TYPE_SNP_NOTARY));
                    pPairedItem->setData(0, Qt::UserRole+1, QVariant(ACTIVITY_TREE_HEADER_SNP));
                    if (!qstrTLA.isEmpty()) {
                        pPairedItem->setData(0, Qt::UserRole+2, QVariant(qstrTLA));
                    }
                    if (!qstrAssetTypeId.isEmpty()) {
                        pPairedItem->setData(0, Qt::UserRole+3, QVariant(qstrAssetTypeId));
                    }
                    if (!qstrAccountId.isEmpty()) {
                        pPairedItem->setData(0, Qt::UserRole+4, QVariant(qstrAccountId));
                    }
                    if (!qstrServerId.isEmpty()) {
                        pPairedItem->setData(0, Qt::UserRole+5, QVariant(qstrServerId));
                    }
                }
                else // Multiple accounts exist for this server ID.
                {
                    QTreeWidgetItem * pPairedItem = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(qstrSnpName) << QString("") << QString(""));
                    items.append(pPairedItem);
                    pPairedItem->setExpanded(true);

                    const QString qstrIconPath = QString(bConnected ? ":/icons/icons/green_dot.png" : ":/icons/icons/red_dot.png");
                    pPairedItem->setIcon(0, QIcon(qstrIconPath));
                    if (bConnected)
                    {
    //                    pPairedItem->setTextColor(0, Qt::black);
    //                    pPairedItem->setTextColor(1, Qt::black);
                    }
                    else
                    {
                        pPairedItem->setTextColor(0, Qt::gray);
                        pPairedItem->setTextColor(1, Qt::gray);
                    }
                    pPairedItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);

                    pPairedItem->setData(0, Qt::UserRole  , QVariant(AC_NODE_TYPE_SNP_NOTARY));
                    pPairedItem->setData(0, Qt::UserRole+1, QVariant(ACTIVITY_TREE_HEADER_SNP));
                    if (!qstrServerId.isEmpty()) {
                        pPairedItem->setData(0, Qt::UserRole+5, QVariant(qstrServerId));
                    }
                    // -------
                    mapIDName::const_iterator ci_accounts = mapAccounts.begin();

                    while (ci_accounts != mapAccounts.end())
                    {
                        const QString qstrAccountId = ci_accounts.key();
                        const std::string str_acct_id = qstrAccountId.toStdString();
                        const std::string str_asset_id = opentxs::OT::App().API().Exec().GetAccountWallet_InstrumentDefinitionID(str_acct_id);
                        const int64_t lBalance = opentxs::OT::App().API().Exec().GetAccountWallet_Balance(str_acct_id);
                        const std::string str_formatted_amount = opentxs::OT::App().API().Exec().FormatAmountWithoutSymbol(str_asset_id, lBalance);
    //                  const std::string str_formatted_amount = opentxs::OT::App().API().Exec().FormatAmount(str_asset_id, lBalance);
                        const QString qstrFormattedAmount = QString::fromStdString(str_formatted_amount);

                        const std::string asset_contract = opentxs::OT::App().API().Exec().GetAssetType_Contract(str_asset_id);
                        opentxs::proto::UnitDefinition contractProto = opentxs::proto::StringToProto<opentxs::proto::UnitDefinition>
                                                                        (opentxs::String(asset_contract));

                        const std::string str_symbol = (contractProto.has_currency() && contractProto.has_symbol()) ? contractProto.symbol() : "";
                        const QString qstrSymbol(QString::fromStdString(str_symbol));
                        const QString qstrAcctName = QString("     %1").arg(QString::fromStdString(opentxs::OT::App().API().Exec().GetAccountWallet_Name(str_acct_id)));
                        const QString qstrAssetTypeId = QString::fromStdString(str_asset_id);
                        const QString qstrTLA = QString::fromStdString(opentxs::OT::App().API().Exec().GetCurrencyTLA(str_asset_id));
                        // ------------------
                        QTreeWidgetItem * pSubItem = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(qstrAcctName) << qstrSymbol << qstrFormattedAmount << qstrTLA);
                        items.append(pSubItem);
                        pSubItem->setExpanded(false);
                        //pSubItem->setIcon(0, QIcon(":/icons/icons/red_dot.png"));
                        if (bConnected)
                        {
        //                    pSubItem->setTextColor(0, Qt::black);
        //                    pSubItem->setTextColor(1, Qt::black);
                        }
                        else
                        {
                            pSubItem->setTextColor(0, Qt::gray);
                            pSubItem->setTextColor(1, Qt::gray);
                        }
                        pSubItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);

                        //todo
                        pSubItem->setData(0, Qt::UserRole  , QVariant(AC_NODE_TYPE_SNP_ACCOUNT));
                        pSubItem->setData(0, Qt::UserRole+1, QVariant(ACTIVITY_TREE_HEADER_SNP));

                        if (!qstrTLA.isEmpty()) {
                            pSubItem->setData(0, Qt::UserRole+2, QVariant(qstrTLA));
                        }
                        if (!qstrAssetTypeId.isEmpty()) {
                            pSubItem->setData(0, Qt::UserRole+3, QVariant(qstrAssetTypeId));
                        }
                        if (!qstrAccountId.isEmpty()) {
                            pSubItem->setData(0, Qt::UserRole+4, QVariant(qstrAccountId));
                        }

                        if (!qstrServerId.isEmpty()) {
                            pSubItem->setData(0, Qt::UserRole+5, QVariant(qstrServerId));
                        }


                        ++ci_accounts;
                    }
                }
            }
            else // found no accounts for this server ID.
            {
                QTreeWidgetItem * pNodeItem = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(qstrSnpName) << QString("") << QString(""));
                items.append(pNodeItem);
                pNodeItem->setExpanded(false);
                const QString qstrIconPath(bConnected ? ":/icons/icons/green_dot.png" : ":/icons/icons/red_dot.png");
                pNodeItem->setIcon(0, QIcon(qstrIconPath));
                pNodeItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);

                pNodeItem->setData(0, Qt::UserRole  , QVariant(AC_NODE_TYPE_SNP_NOTARY));
                pNodeItem->setData(0, Qt::UserRole+1, QVariant(ACTIVITY_TREE_HEADER_SNP));
                if (!qstrServerId.isEmpty()) {
                    pNodeItem->setData(0, Qt::UserRole+5, QVariant(qstrServerId));
                }
            }
        }
    }
    // ------------------------------------------------------------------------

    mapOfMapIDName bigMapAccounts;

    this->GetAccountIdMapsByServerId(bigMapAccounts, false); // false means, only give me non-paired servers.

    if (bigMapAccounts.size() > 0)
    {
        // ----------------------------------------
        QTreeWidgetItem * pHostedAccountsItem = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(tr("Hosted Accounts")));
        items.append(pHostedAccountsItem);
        pHostedAccountsItem->setExpanded(true);
        pHostedAccountsItem->setData(0, Qt::UserRole  , QVariant(AC_NODE_TYPE_HEADER));
        pHostedAccountsItem->setData(0, Qt::UserRole+1, QVariant(ACTIVITY_TREE_HEADER_HOSTED));
        pHostedAccountsItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
        pHostedAccountsItem->setTextColor(0, Qt::white);
        pHostedAccountsItem->setTextColor(1, Qt::white);
        pHostedAccountsItem->setBackgroundColor(0, Qt::gray);
        pHostedAccountsItem->setBackgroundColor(1, Qt::gray);
        pHostedAccountsItem->setBackgroundColor(2, Qt::gray);
        pHostedAccountsItem->setBackgroundColor(3, Qt::gray);
        pHostedAccountsItem->setFlags(pHostedAccountsItem->flags()&~Qt::ItemIsSelectable);
        // ----------------------------------------
        // From Vaage:
        // Retrieve a list of hosted accounts for each server.
        // For each server, if the list is empty, skip it.
        // If the list only contains one account, display
        // the balance directly on the same line as the server name.
        // Only give it a separate line if the user has gone
        // out of his way to name the account.
        // If the user has named it, or if there are multiple accounts
        // for that server, then display the balances below that server
        // on the tree.

        mapOfMapIDName::const_iterator ci_servers = bigMapAccounts.begin();

        while (ci_servers != bigMapAccounts.end()) {
            const QString qstrServerId = ci_servers.key();
            const mapIDName & mapAccounts = ci_servers.value();
            const std::string str_server_id = qstrServerId.toStdString();
            const bool bIsConnectionAlive = opentxs::OT::App().API().Exec().CheckConnection(str_server_id);

            QString qstrServerName = QString::fromStdString(opentxs::OT::App().API().Exec().GetServer_Name(str_server_id));

            if (1 == mapAccounts.size())
            {
                const QString qstrAccountId = mapAccounts.begin().key();
                const std::string str_acct_id = qstrAccountId.toStdString();
                const std::string str_asset_id = opentxs::OT::App().API().Exec().GetAccountWallet_InstrumentDefinitionID(str_acct_id);
                const int64_t lBalance = opentxs::OT::App().API().Exec().GetAccountWallet_Balance(str_acct_id);

                const std::string str_formatted_amount = opentxs::OT::App().API().Exec().FormatAmountWithoutSymbol(str_asset_id, lBalance);
//              const std::string str_formatted_amount = opentxs::OT::App().API().Exec().FormatAmount(str_asset_id, lBalance);
                const QString qstrFormattedAmount = QString::fromStdString(str_formatted_amount);

                const std::string asset_contract = opentxs::OT::App().API().Exec().GetAssetType_Contract(str_asset_id);
                opentxs::proto::UnitDefinition contractProto = opentxs::proto::StringToProto<opentxs::proto::UnitDefinition>
                                                                (opentxs::String(asset_contract));

                const std::string str_symbol = (contractProto.has_currency() && contractProto.has_symbol()) ? contractProto.symbol() : "";
                const QString qstrSymbol(QString::fromStdString(str_symbol));
                const QString qstrAssetTypeId = QString::fromStdString(str_asset_id);
                const QString qstrTLA = QString::fromStdString(opentxs::OT::App().API().Exec().GetCurrencyTLA(str_asset_id));
                // ------------------
                QTreeWidgetItem * pHostedItem = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(qstrServerName) << qstrSymbol << qstrFormattedAmount << qstrTLA);
                items.append(pHostedItem);
                pHostedItem->setExpanded(false);

                const QString qstrIconPath = QString(bIsConnectionAlive ? ":/icons/icons/green_dot.png" : ":/icons/icons/red_dot.png");
                pHostedItem->setIcon(0, QIcon(qstrIconPath));
                if (bIsConnectionAlive)
                {
//                    pHostedItem->setTextColor(0, Qt::black);
//                    pHostedItem->setTextColor(1, Qt::black);
                }
                else
                {
                    pHostedItem->setTextColor(0, Qt::gray);
                    pHostedItem->setTextColor(1, Qt::gray);
                }
                pHostedItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);

                pHostedItem->setData(0, Qt::UserRole  , QVariant(AC_NODE_TYPE_HOSTED_NOTARY));
                pHostedItem->setData(0, Qt::UserRole+1, QVariant(ACTIVITY_TREE_HEADER_HOSTED));
                if (!qstrTLA.isEmpty()) {
                    pHostedItem->setData(0, Qt::UserRole+2, QVariant(qstrTLA));
                }
                if (!qstrAssetTypeId.isEmpty()) {
                    pHostedItem->setData(0, Qt::UserRole+3, QVariant(qstrAssetTypeId));
                }
                if (!qstrAccountId.isEmpty()) {
                    pHostedItem->setData(0, Qt::UserRole+4, QVariant(qstrAccountId));
                }
                if (!qstrServerId.isEmpty()) {
                    pHostedItem->setData(0, Qt::UserRole+5, QVariant(qstrServerId));
                }
            }
            else // Multiple accounts exist for this server ID.
            {
                QTreeWidgetItem * pHostedItem = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(qstrServerName));
                items.append(pHostedItem);
                pHostedItem->setExpanded(true);

                const QString qstrIconPath = QString(bIsConnectionAlive ? ":/icons/icons/green_dot.png" : ":/icons/icons/red_dot.png");
                pHostedItem->setIcon(0, QIcon(qstrIconPath));
                if (bIsConnectionAlive)
                {
//                    pHostedItem->setTextColor(0, Qt::black);
//                    pHostedItem->setTextColor(1, Qt::black);
                }
                else
                {
                    pHostedItem->setTextColor(0, Qt::gray);
                    pHostedItem->setTextColor(1, Qt::gray);
                }
                pHostedItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);

                pHostedItem->setData(0, Qt::UserRole  , QVariant(AC_NODE_TYPE_HOSTED_NOTARY));
                pHostedItem->setData(0, Qt::UserRole+1, QVariant(ACTIVITY_TREE_HEADER_HOSTED));
                if (!qstrServerId.isEmpty()) {
                    pHostedItem->setData(0, Qt::UserRole+5, QVariant(qstrServerId));
                }

                // -------
                mapIDName::const_iterator ci_accounts = mapAccounts.begin();

                while (ci_accounts != mapAccounts.end())
                {
                    const QString qstrAccountId = ci_accounts.key();
                    const std::string str_acct_id = qstrAccountId.toStdString();
                    const std::string str_asset_id = opentxs::OT::App().API().Exec().GetAccountWallet_InstrumentDefinitionID(str_acct_id);
                    const int64_t lBalance = opentxs::OT::App().API().Exec().GetAccountWallet_Balance(str_acct_id);
                    const std::string str_formatted_amount = opentxs::OT::App().API().Exec().FormatAmountWithoutSymbol(str_asset_id, lBalance);
//                  const std::string str_formatted_amount = opentxs::OT::App().API().Exec().FormatAmount(str_asset_id, lBalance);
                    const QString qstrFormattedAmount = QString::fromStdString(str_formatted_amount);

                    const std::string asset_contract = opentxs::OT::App().API().Exec().GetAssetType_Contract(str_asset_id);
                    opentxs::proto::UnitDefinition contractProto = opentxs::proto::StringToProto<opentxs::proto::UnitDefinition>
                                                                    (opentxs::String(asset_contract));

                    const std::string str_symbol = (contractProto.has_currency() && contractProto.has_symbol()) ? contractProto.symbol() : "";
                    const QString qstrSymbol(QString::fromStdString(str_symbol));
                    const QString qstrAcctName = QString("     %1").arg(QString::fromStdString(opentxs::OT::App().API().Exec().GetAccountWallet_Name(str_acct_id)));
                    const QString qstrAssetTypeId = QString::fromStdString(str_asset_id);
                    const QString qstrTLA = QString::fromStdString(opentxs::OT::App().API().Exec().GetCurrencyTLA(str_asset_id));
                    // ------------------
                    QTreeWidgetItem * pSubItem = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(qstrAcctName) << qstrSymbol << qstrFormattedAmount << qstrTLA);
                    items.append(pSubItem);
                    pSubItem->setExpanded(false);
                    //pSubItem->setIcon(0, QIcon(":/icons/icons/red_dot.png"));
                    if (bIsConnectionAlive)
                    {
    //                    pSubItem->setTextColor(0, Qt::black);
    //                    pSubItem->setTextColor(1, Qt::black);
                    }
                    else
                    {
                        pSubItem->setTextColor(0, Qt::gray);
                        pSubItem->setTextColor(1, Qt::gray);
                    }
                    pSubItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);

                    pSubItem->setData(0, Qt::UserRole  , QVariant(AC_NODE_TYPE_HOSTED_ACCOUNT));
                    pSubItem->setData(0, Qt::UserRole+1, QVariant(ACTIVITY_TREE_HEADER_HOSTED));
                    if (!qstrTLA.isEmpty()) {
                        pSubItem->setData(0, Qt::UserRole+2, QVariant(qstrTLA));
                    }
                    if (!qstrAssetTypeId.isEmpty()) {
                        pSubItem->setData(0, Qt::UserRole+3, QVariant(qstrAssetTypeId));
                    }
                    if (!qstrAccountId.isEmpty()) {
                        pSubItem->setData(0, Qt::UserRole+4, QVariant(qstrAccountId));
                    }
                    if (!qstrServerId.isEmpty()) {
                        pSubItem->setData(0, Qt::UserRole+5, QVariant(qstrServerId));
                    }

                    ++ci_accounts;
                }
            }

            ++ci_servers;
        }



    }
    // ----------------------------------------

//    {
//    QTreeWidgetItem * pHostedItem = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList("Ruritarian Reserve") << "BTC 1.686 46731");
//    items.append(pHostedItem);
//    pHostedItem->setExpanded(false);
//    pHostedItem->setIcon(0, QIcon(":/icons/icons/red_dot.png"));
//    pHostedItem->setTextColor(0, Qt::gray);
//    pHostedItem->setTextColor(1, Qt::gray);
//    pHostedItem->setData(0, Qt::UserRole, QVariant(0));
//    pHostedItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
//    }
//    {
//    QTreeWidgetItem * pHostedItem = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList("Nakamoto Market") << "BTC 0.000 00000");
//    items.append(pHostedItem);
//    pHostedItem->setExpanded(false);
//    pHostedItem->setIcon(0, QIcon(":/icons/icons/green_dot.png"));
//    pHostedItem->setData(0, Qt::UserRole, QVariant(0));
//    pHostedItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
//    }

    mapIDName mapContacts;

    if (MTContactHandler::getInstance()->GetOpentxsContacts(mapContacts))
    {
        // ----------------------------------------
        QTreeWidgetItem * pContactsItem = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(tr("Contacts")));
        items.append(pContactsItem);
        pContactsItem->setExpanded(true);
        pContactsItem->setData(0, Qt::UserRole  , QVariant(AC_NODE_TYPE_HEADER));
        pContactsItem->setData(0, Qt::UserRole+1, QVariant(ACTIVITY_TREE_HEADER_CONTACTS));
        pContactsItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
        pContactsItem->setTextColor(0, Qt::white);
        pContactsItem->setTextColor(1, Qt::white);
        pContactsItem->setBackgroundColor(0, Qt::gray);
        pContactsItem->setBackgroundColor(1, Qt::gray);
        pContactsItem->setBackgroundColor(2, Qt::gray);
        pContactsItem->setBackgroundColor(3, Qt::gray);
        pContactsItem->setFlags(pContactsItem->flags()&~Qt::ItemIsSelectable);
        // ----------------------------------------
        for (mapIDName::iterator ii = mapContacts.begin(); ii != mapContacts.end(); ii++)
        {
            QString qstrContactID   = ii.key();
            QString qstrTemp = ii.value();
            QString qstrContactName = QString("CONTACT_NAME_EMPTY");
            if (!qstrTemp.isEmpty())
            {
                qstrContactName = qstrTemp;
            }
            // ------------------------------------
            QTreeWidgetItem * pTopItem = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(qstrContactName));

            pTopItem->setData(0, Qt::UserRole  , QVariant(AC_NODE_TYPE_CONTACT));
            pTopItem->setData(0, Qt::UserRole+1, QVariant(ACTIVITY_TREE_HEADER_CONTACTS));

            if (!qstrContactID.isEmpty()) {
                pTopItem->setData(0, Qt::UserRole+6, QVariant(qstrContactID));
            }

            pTopItem->setExpanded(true);
            //pTopItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
            items.append(pTopItem);
        }
        // ----------------------------------------
        if (items.count() > 0)
        {
            pTreeWidgetAccounts->insertTopLevelItems(0, items);
//            pTreeWidgetAccounts->resizeColumnToContents(0);
        }
    }

    pTreeWidgetAccounts->resizeColumnToContents(0);
    pTreeWidgetAccounts->resizeColumnToContents(1);
    pTreeWidgetAccounts->resizeColumnToContents(2);
    pTreeWidgetAccounts->resizeColumnToContents(3);
    // ----------------------------------------
    // Make sure the same item that was selected before, is selected again.
    // (If it still exists, which it probably does.)

    QTreeWidgetItem * previous = pTreeWidgetAccounts->currentItem();

    // In this case, just select the first thing on the list.
    if ( qstrCurrentTLA_.isEmpty() && qstrCurrentNotary_.isEmpty() && qstrCurrentAccount_.isEmpty() && qstrCurrentContact_.isEmpty() )
    {
        if (pTreeWidgetAccounts->topLevelItemCount() > 0)
            pTreeWidgetAccounts->setCurrentItem(pTreeWidgetAccounts->topLevelItem(0));
    }
    else // Find the one that was selected before the refresh.
    {
        bool bFoundIt = false;

        QTreeWidgetItemIterator it(pTreeWidgetAccounts);

        while (*it)
        {
            const int      nNodeType       = (*it)->data(0, Qt::UserRole  ).toInt();
            const int      nHeader         = (*it)->data(0, Qt::UserRole+1).toInt();
            const QVariant qvarTLA         = (*it)->data(0, Qt::UserRole+2);
            const QVariant qvarAssetTypeId = (*it)->data(0, Qt::UserRole+3);
            const QVariant qvarAccountId   = (*it)->data(0, Qt::UserRole+4);
            const QVariant qvarServerId    = (*it)->data(0, Qt::UserRole+5);
            const QVariant qvarContactId   = (*it)->data(0, Qt::UserRole+6);
            // ------------------------------------------------------------
            const QString  qstrTLA         = qvarTLA.isValid()         ? qvarTLA.toString()         : "";
            const QString  qstrAssetTypeId = qvarAssetTypeId.isValid() ? qvarAssetTypeId.toString() : "";
            const QString  qstrServerId    = qvarServerId.isValid()    ? qvarServerId.toString()    : "";
            const QString  qstrAccountId   = qvarAccountId.isValid()   ? qvarAccountId.toString()   : "";
            const QString  qstrContactId   = qvarContactId.isValid()   ? qvarContactId.toString()   : "";
            // ------------------------------------------------------------
            if (    (0 == qstrTLA.compare(qstrCurrentTLA_ ))
                 && (0 == qstrServerId.compare(qstrCurrentNotary_))
                 && (0 == qstrAccountId.compare(qstrCurrentAccount_))
                 && (0 == qstrContactId.compare(qstrCurrentContact_)) )
            {
                bFoundIt = true;
                pTreeWidgetAccounts->setCurrentItem(*it);
                break;
            }
            // ------------------------
            ++it;
        } //while
        if (!bFoundIt)
        {
            if (pTreeWidgetAccounts->topLevelItemCount() > 0)
                pTreeWidgetAccounts->setCurrentItem(pTreeWidgetAccounts->topLevelItem(0));
        }
    }
//  pTreeWidgetAccounts->expandAll();
    // ----------------------------------------
    pTreeWidgetAccounts->blockSignals(false);
    // ----------------------------------------
    on_treeWidgetAccounts_currentItemChanged(pTreeWidgetAccounts->currentItem(), previous);
}

//void Activity::RefreshAccountList()
//{
//    QTreeWidget * pTreeWidgetAccounts = ui->treeWidgetAccounts;
//    if (nullptr == pTreeWidgetAccounts) {
//        return;
//    }
//    pTreeWidgetAccounts->blockSignals(true);
//    // ----------------------------------------------
//    // Clear it
//    pTreeWidgetAccounts->clear();
//    // ----------------------------------------------
//    // Re-populate it

//    int nIndex = 0;
//    QListWidgetItem * pItem = nullptr;

//    pTreeWidgetAccounts->addItem("Bitcoin");
//    pItem = pTreeWidgetAccounts->item(nIndex);
//    nIndex++;
//    pItem->setTextColor(Qt::white);
//    pItem->setBackgroundColor(Qt::gray);
//    // ----------------------------------------------
//    /*Loop through Blockchain Types for the totals*/ {
//        pTreeWidgetAccounts->addItem("BTC                        " + QString("5.038 47164"));
//        pItem = pTreeWidgetAccounts->item(nIndex);
//        nIndex++;
//        pItem->setTextColor(Qt::white);
//        pItem->setBackgroundColor(Qt::gray);
//        // ----------------------------------------------
//        pTreeWidgetAccounts->addItem("USD                        " + QString("$3,275.01"));
//        pItem = pTreeWidgetAccounts->item(nIndex);
//        nIndex++;
//        pItem->setTextColor(Qt::darkGray);
//        pItem->setBackgroundColor(Qt::gray);
//    }
//    // -----------------------------
//    pTreeWidgetAccounts->addItem("*GRN* Mac Desktop       " + QString("0.000 00000"));
//    pItem = pTreeWidgetAccounts->item(nIndex);
//    nIndex++;
//    pItem->setTextColor(Qt::white);
//    pItem->setBackgroundColor(Qt::darkGray);
//    // -----------------------------
//    pTreeWidgetAccounts->addItem("Your Stash Node");
//    pItem = pTreeWidgetAccounts->item(nIndex);
//    nIndex++;
//    pItem->setTextColor(Qt::lightGray);
//    pItem->setBackgroundColor(Qt::gray);
//    // -----------------------------
//    /*Loop through all the paired nodes*/ {
//        pTreeWidgetAccounts->addItem("*GRN* John Galt ");
//        pItem = pTreeWidgetAccounts->item(nIndex);
//        nIndex++;
//        pItem->setTextColor(Qt::white);
//        pItem->setBackgroundColor(Qt::darkGray);
//        // -----------------------------
//        pTreeWidgetAccounts->addItem("      Primary         " + QString("0.493 87640"));
//        pItem = pTreeWidgetAccounts->item(nIndex);
//        nIndex++;
//        pItem->setTextColor(Qt::lightGray);
//        pItem->setBackgroundColor(Qt::darkGray);
//        // -----------------------------
//        pTreeWidgetAccounts->addItem("      Savings         " + QString("2.730 67371"));
//        pItem = pTreeWidgetAccounts->item(nIndex);
//        nIndex++;
//        pItem->setTextColor(Qt::lightGray);
//        pItem->setBackgroundColor(Qt::darkGray);
//    }
//    // -----------------------------
//    pTreeWidgetAccounts->addItem("Hosted Accounts");
//    pItem = pTreeWidgetAccounts->item(nIndex);
//    nIndex++;
//    pItem->setTextColor(Qt::lightGray);
//    pItem->setBackgroundColor(Qt::gray);
//    // -----------------------------
//    /*Loop through all the hosted accounts*/ {
//        pTreeWidgetAccounts->addItem("*RED* Ruritarian Reserve   " + QString("1.686 46731"));
//        pItem = pTreeWidgetAccounts->item(nIndex);
//        nIndex++;
//        pItem->setTextColor(Qt::gray);
//        pItem->setBackgroundColor(Qt::darkGray);
//        // -----------------------------
//        pTreeWidgetAccounts->addItem("*GRN* John Galt                " + QString("0.000 00000"));
//        pItem = pTreeWidgetAccounts->item(nIndex);
//        nIndex++;
//        pItem->setTextColor(Qt::white);
//        pItem->setBackgroundColor(Qt::darkGray);
//    }
//    // -----------------------------
////    for (int i = 0; i < 10; i++)
////    {
////        if (0 == i) {
////            pTreeWidgetAccounts->addItem("MAIN BALANCE: " + QString::number(i));
////        }
////        else
////            pTreeWidgetAccounts->addItem("      Item " + QString::number(i));
////    }
//    // ----------------------------------------------
//    pTreeWidgetAccounts->blockSignals(false);
//}





//resume
//void Activity::on_listWidgetConversations_currentRowChanged


void Activity::on_listWidgetConversations_currentRowChanged(int currentRow)
{
//    if ((-1) == currentRow)
//        ui->listWidgetConversation->setEnabled(false);
//    else
//        ui->listWidgetConversation->setEnabled(true);
    // -------------------
    RefreshConversationDetails(currentRow);
    // -------------------
//    ui->tableViewConversation->resizeColumnsToContents();
//    ui->tableViewConversation->resizeRowsToContents();
//    ui->tableViewConversation->horizontalHeader()->setStretchLastSection(true);


//    auto qs = ui->tableViewConversation->sizeHint();
//    ui->tableViewConversation->resize(qs);

}
//        message StorageThread {
//            optional uint32 version = 1;
//            optional string id = 2;
//            repeated string participant = 3;
//            repeated StorageThreadItem item = 4;
//        }
//        enum class StorageBox : std::uint8_t {
//            SENTPEERREQUEST = 0,
//            INCOMINGPEERREQUEST = 1,
//            SENTPEERREPLY = 2,
//            INCOMINGPEERREPLY = 3,
//            FINISHEDPEERREQUEST = 4,
//            FINISHEDPEERREPLY = 5,
//            PROCESSEDPEERREQUEST = 6,
//            PROCESSEDPEERREPLY = 7,
//            MAILINBOX = 8,
//            MAILOUTBOX = 9
//        };


void Activity::ClearListWidgetConversations()
{
    ui->listWidgetConversations->blockSignals(true);
    // -----------------------------------
    ui->listWidgetConversations->clear();
    // -----------------------------------
    ui->listWidgetConversations->setCurrentRow(-1, 0);
    ui->listWidgetConversations->blockSignals(false);

    on_listWidgetConversations_currentRowChanged(-1);
}

//        message StorageThread {
//            optional uint32 version = 1;
//            optional string id = 2;
//            repeated string participant = 3;
//            repeated StorageThreadItem item = 4;
//        }
//        message StorageThreadItem {
//            optional uint32 version = 1;
//            optional string id = 2;
//            optional uint64 index = 3;
//            optional uint64 time = 4;
//            optional uint32 box = 5;
//            optional string account = 6;
//            optional bool unread = 7;
//        }
//        enum class StorageBox : std::uint8_t {
//            SENTPEERREQUEST = 0,
//            INCOMINGPEERREQUEST = 1,
//            SENTPEERREPLY = 2,
//            INCOMINGPEERREPLY = 3,
//            FINISHEDPEERREQUEST = 4,
//            FINISHEDPEERREPLY = 5,
//            PROCESSEDPEERREQUEST = 6,
//            PROCESSEDPEERREPLY = 7,
//            MAILINBOX = 8,
//            MAILOUTBOX = 9
//        };

void Activity::PopulateConversationsForNym(QListWidget * pListWidgetConversations, int & nIndex, const std::string & str_my_nym_id)
{
    const int  nNymCount = opentxs::OT::App().API().Exec().GetNymCount();
    OT_ASSERT(nNymCount > 0);
    const bool bOnlyOneNymInWallet = (1 == nNymCount);
    // ------------------
    MTNameLookupQT theLookup;
    std::string str_my_nym_name ("");
    // ------------------
    if (bOnlyOneNymInWallet)
    {
        str_my_nym_name = "Me";
    }
    else
    {
        str_my_nym_name = theLookup.GetNymName(str_my_nym_id, "");

        if (str_my_nym_name.empty()) {
            str_my_nym_name = "Me";
        }
    }
//  const auto response = OT::App().Contact().NewContact(label, opentxs::Identifier{hisnym}, opentxs::PaymentCode{paymentCode});
//  const auto pContact = OT::App().Contact().Contact(opentxs::Identifier{contact});

    //NOTE: no point here, since the above lookup already uses the below code.
    //      It finds the contact based on NymID and then gets the Label() for that contact.
//    if (str_my_nym_name.empty()) { // still empty?
//        get contact ID from Nym ID here. Then get pointer to contact.
//        const auto pContact = OT::App().Contact().Contact(opentxs::Identifier{contact});
//        pContact->
//    }
    // ------------------------
    const QString qstrMyNymName = QString::fromStdString(str_my_nym_name);
    // ------------------------
    opentxs::ObjectList threadList = opentxs::OT::App().Activity().Threads(opentxs::Identifier{str_my_nym_id});
    opentxs::ObjectList::const_iterator ci = threadList.begin();

    while (threadList.end() != ci)
    {
        const std::pair<std::string, std::string> & threadInfo = *ci;

        const std::string & str_thread_id = threadInfo.first;
        const std::string & str_thread_name = threadInfo.second;
        // ----------------------------------------------
        std::shared_ptr<opentxs::proto::StorageThread> thread;
        opentxs::OT::App().DB().Load(str_my_nym_id, str_thread_id, thread);
        // ----------------------------------------------
        if (!thread) {
            ++ci;
            continue;
        }
        // ----------------------------------------------
        if (thread->participant_size() <= 0) {
            ++ci;
            continue;
        }
        // ----------------------------------------------
        QString qstrDisplayName = bOnlyOneNymInWallet
                ? QString("%1").arg(QString::fromStdString(str_thread_name))
                : QString("%1 in convo with: %2")
                  .arg(qstrMyNymName)
                  .arg(QString::fromStdString(str_thread_name));

        pListWidgetConversations->addItem(qstrDisplayName);
        QListWidgetItem * pItem = pListWidgetConversations->item(nIndex);
        nIndex++;
        pItem->setTextColor(Qt::white);
        const QString qstrThreadId = QString::fromStdString(str_thread_id);
        const QString qstrMyNymId = QString::fromStdString(str_my_nym_id);
        pItem->setData(Qt::UserRole+1, QVariant(qstrMyNymId));
        pItem->setData(Qt::UserRole+2, QVariant(qstrThreadId));
        pItem->setBackgroundColor((nIndex%2 == 0) ? Qt::gray : Qt::darkGray);
        // ----------------------------------------------
        ++ci;
    }
}

void Activity::RefreshConversationsTab()
{
    QListWidget * pListWidgetConversations = ui->listWidgetConversations;
    if (nullptr == pListWidgetConversations) {
        return;
    }
    pListWidgetConversations->blockSignals(true);
    // ----------------------------------------------
    // Clear it
    pListWidgetConversations->clear();
    // ----------------------------------------------
    // Re-populate it

    int nIndex = 0;
//    QListWidgetItem * pItem = nullptr;

//    pListWidgetConversations->addItem("Cliff");
//    pItem = pListWidgetConversations->item(nIndex);
//    nIndex++;
//    pItem->setTextColor(Qt::white);
//    pItem->setBackgroundColor(Qt::gray);
//    // ----------------------------------------------
//    pListWidgetConversations->addItem("Justus");
//    pItem = pListWidgetConversations->item(nIndex);
//    nIndex++;
//    pItem->setTextColor(Qt::white);
//    pItem->setBackgroundColor(Qt::darkGray);
//    // ----------------------------------------------
//    pListWidgetConversations->addItem("Hiro and Cliff");
//    pItem = pListWidgetConversations->item(nIndex);
//    nIndex++;
//    pItem->setTextColor(Qt::white);
//    pItem->setBackgroundColor(Qt::darkGray);
    // ----------------------------------------------
    // Conversational threads.
    //
//  const std::string str_nym_id = Moneychanger::It()->get_default_nym_id().toStdString();

    int nNymCount = opentxs::OT::App().API().Exec().GetNymCount();
    // ----------------------------------------------------
    for (int ii = 0; ii < nNymCount; ++ii)
    {
        const std::string str_nym_id = opentxs::OT::App().API().Exec().GetNym_ID(ii);

        this->PopulateConversationsForNym(pListWidgetConversations, nIndex, str_nym_id);
    }
    // ----------------------------------------------------
    pListWidgetConversations->blockSignals(false);
}





static void setup_tableview_conversation(QTableView * pView)
{
    pView->setSortingEnabled(true);
    pView->resizeColumnsToContents();
//  pView->resizeRowsToContents();
    pView->horizontalHeader()->setStretchLastSection(true);
    pView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pView->sortByColumn(0, Qt::DescendingOrder); // The timestamp ends up at index 0 in this proxy view.
    pView->setContextMenuPolicy(Qt::CustomContextMenu);
    pView->verticalHeader()->hide();
    pView->horizontalHeader()->hide();
    pView->setAlternatingRowColors(true);
    pView->setSelectionBehavior(QAbstractItemView::SelectRows);
}


static void setup_tableview(QTableView * pView, QAbstractItemModel * pProxyModel)
{
    ActivityPaymentsProxyModel * pPmntProxyModel = static_cast<ActivityPaymentsProxyModel *>(pProxyModel);
    pPmntProxyModel->setTableView(pView);
    pView->setModel(pProxyModel);

    pView->setSortingEnabled(true);
    pView->resizeColumnsToContents();
    pView->resizeRowsToContents();
    pView->horizontalHeader()->setStretchLastSection(true);
    pView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    pView->sortByColumn(7, Qt::DescendingOrder); // The timestamp ends up at index 7 in all the proxy views.

    pView->setContextMenuPolicy(Qt::CustomContextMenu);
    pView->verticalHeader()->hide();
    pView->setAlternatingRowColors(true);
    pView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    pView->setSelectionBehavior(QAbstractItemView::SelectRows);
}




//QSharedPointer<QSqlQueryMessages>  pModelMessages_;
//QSharedPointer<ConvMsgsProxyModel> pThreadItemsProxyModel_;

void Activity::resetConversationItemsDataModel(const bool bProvidedIds/*=false*/,
                                               const QString * pstrMyNymID/*=nullptr*/,
                                               const QString * pstrThreadID/*=nullptr*/)
{
    if ((nullptr == ui) || (nullptr == ui->tableViewConversation))
    {
        return;
        // Must be early in the run, no UI elements instantiated yet.
    }
    // --------------------------------------------------------
    if (bProvidedIds && (nullptr == pstrMyNymID || nullptr == pstrThreadID))
    {
        qDebug() << "resetConversationItemsDataModel: Apparently Ids were expected but not actually provided. Returning. (Failed). Should never happen.";
        return;
    }
    // By this point we know if bProvidedIds is true,
    // that neither of the optional parameters are nullptr.
    // --------------------------------------------------------
    QSharedPointer<QSqlQueryMessages> pNewSourceModel;

    if (!bProvidedIds)
    {
        pNewSourceModel.reset(new QSqlQueryMessages(0)); // An empty one, since there's no IDs for a real one.
    }
    else // bProvidedIds is definitely true...
    {
        pNewSourceModel = DBHandler::getInstance()->getConversationItemModel(*pstrMyNymID, *pstrThreadID);
    }
    // --------------------------------------------------------
    QSharedPointer<ConvMsgsProxyModel> pThreadItemsProxyModel{new ConvMsgsProxyModel}; // A new proxy
    pThreadItemsProxyModel->setSourceModel(pNewSourceModel.data());
    // ---------------------------------
    // Todo: any other setup / filters for the proxy model.
    // ---------------------------------
    pModelMessages_ = pNewSourceModel;
    pThreadItemsProxyModel_ = pThreadItemsProxyModel;
    // ---------------------------------
    pThreadItemsProxyModel->setTableView(ui->tableViewConversation);
    ui->tableViewConversation->setModel(pThreadItemsProxyModel.data());
}


void Activity::RefreshConversationDetails(int nRow)
{
    if (nRow <  0)
    {
        resetConversationItemsDataModel();
    }
    else // nRow >=0
    {
        QListWidgetItem * conversation_widget = ui->listWidgetConversations->currentItem();

        if (nullptr == conversation_widget) {
            resetConversationItemsDataModel();
            return;
        }

        const QVariant qvarMyNymId  = conversation_widget->data(Qt::UserRole+1);
        const QVariant qvarThreadId = conversation_widget->data(Qt::UserRole+2);

        const QString  qstrMyNymId  = qvarMyNymId.isValid()  ? qvarMyNymId.toString()  : QString("");
        const QString  qstrThreadId = qvarThreadId.isValid() ? qvarThreadId.toString() : QString("");

        if (qstrMyNymId.isEmpty() || qstrThreadId.isEmpty())
        {
            resetConversationItemsDataModel();
            return; // Should never happen.
        }
        // ----------------------------------------------
        resetConversationItemsDataModel(true, &qstrMyNymId, &qstrThreadId);

        return;
        // ----------------------------------------------
        // We don't filter using the proxy model anymore.
        // At least, we don't filter by My Nym and Thread Id.
        // That's now done in the source model, since we query
        // it that way from the database.
        //
//        std::shared_ptr<opentxs::proto::StorageThread> thread;
//        opentxs::OT::App().DB().Load(str_my_nym_id, str_thread_id, thread);
//        // ----------------------------------------------
//        if (!thread) {
//            return;
//        }
//        // ----------------------------------------------
//        if (pThreadItemsProxyModel_)
//        {
//            // Note: until we have group convos, the Thread Id IS the Contact ID.
//            //
//            pThreadItemsProxyModel_->setFilterTopLevel_Convo(str_my_nym_id, str_thread_id);
//        }
        // ----------------------------------------------
        // Below is how we populated the OLD control (removed from UI already):

//        int nConversationIndex = 0;
//        for(const auto & item : thread->item())
//        {
//            bool bIncoming = false;
//            if (opentxs::StorageBox::MAILINBOX == static_cast<opentxs::StorageBox>(item.box())) {
//                bIncoming = true;
//            }
////          message StorageThreadItem {
////            optional uint32 version = 1;
////            optional string id = 2;
////            optional uint64 index = 3;
////            optional uint64 time = 4;
////            optional uint32 box = 5;
////            optional string account = 6;
////            optional bool unread = 7;
////            }
//            const std::string item_id = item.id();
////          const uint64_t item_index = item.index();
////          const uint64_t item_time = item.time();
////          const std::string str_item_account = item.account();
//            // ----------------------------------------------
//            const QString qstrItemId = QString::fromStdString(item_id);
//            // First let's see if we already imported it yet:
//            QString qstrContents = MTContactHandler::getInstance()->GetMessageBody(qstrItemId);
//            // Okay, maybe we haven't imported it yet?
//            // Let's try and grab a copy from the actual opentxs box. (inmail or outmail).
//            //
//            if (qstrContents.isEmpty()) {
//                const std::string str_item_contents = bIncoming ?
//                            opentxs::SwigWrap::GetNym_MailContentsByIndex   (str_my_nym_id, item_id) :
//                            opentxs::SwigWrap::GetNym_OutmailContentsByIndex(str_my_nym_id, item_id);
//                qstrContents = str_item_contents.empty() ? QString("") : QString::fromStdString(str_item_contents);
//            }
//            // ----------------------------------------------
//            QString qstrSubject{"subject:"};
//            const bool bHasContents = !qstrContents.isEmpty();
//            const bool bHasSubject  = bHasContents && qstrContents.startsWith(qstrSubject, Qt::CaseInsensitive);
//            if (bHasContents && !bHasSubject) {
//                // Notice I'm passing 'false' for bAddedByHand. That's because the
//                // item is being added from looping through an existing conversation.
//                if (AddItemToConversation(nConversationIndex, qstrContents, bIncoming, false)) {
//                    nConversationIndex++;
//                }
//            }
//        } // for
        // ----------------------------------------------
    } // else nRow >=0
}



void Activity::dialog(int nSourceRow/*=-1*/, int nFolder/*=-1*/)
{
    if (!already_init)
    {
        ui->horizontalLayout_4->setAlignment(ui->checkBoxSearchConversations, Qt::AlignRight);
        ui->horizontalLayout_6->setAlignment(ui->checkBoxSearchPayments,      Qt::AlignRight);
        ui->horizontalLayout_6->setAlignment(ui->toolButtonMyIdentity,        Qt::AlignLeft);
        ui->horizontalLayout_4->setAlignment(ui->toolButtonMyIdentity2,       Qt::AlignLeft);

        ui->checkBoxPending->setVisible(false);
        ui->listWidgetPending->setVisible(false);

        setup_tableview_conversation(ui->tableViewConversation);

        ui->treeWidgetAccounts->header()->close();
        ui->treeWidgetAccounts->setRootIsDecorated(false);

        ui->treeWidgetAccounts->setContextMenuPolicy(Qt::CustomContextMenu);

        connect(ui->treeWidgetAccounts, SIGNAL(customContextMenuRequested(const QPoint &)),
                this, SLOT(on_treeWidgetAccounts_customContextMenuRequested(const QPoint &)));

        // ******************************************************

        QPointer<ModelPayments> pModel = DBHandler::getInstance()->getPaymentModel();

        if (pModel)
        {
            pPmntProxyModelOutbox_ = new ActivityPaymentsProxyModel;
            pPmntProxyModelInbox_  = new ActivityPaymentsProxyModel;
            pPmntProxyModelInbox_ ->setSourceModel(pModel);
            pPmntProxyModelOutbox_->setSourceModel(pModel);
            pPmntProxyModelOutbox_->setFilterFolder(0);
            pPmntProxyModelInbox_ ->setFilterFolder(1);
            // ---------------------------------
            setup_tableview(ui->tableViewSent, pPmntProxyModelOutbox_);
            setup_tableview(ui->tableViewReceived, pPmntProxyModelInbox_);
            // ---------------------------------
            QItemSelectionModel *sm1 = ui->tableViewSent->selectionModel();
            connect(sm1, SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
                    this, SLOT(on_tableViewSentSelectionModel_currentRowChanged(QModelIndex,QModelIndex)));
            QItemSelectionModel *sm2 = ui->tableViewReceived->selectionModel();
            connect(sm2, SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
                    this, SLOT(on_tableViewReceivedSelectionModel_currentRowChanged(QModelIndex,QModelIndex)));
        }
        // --------------------------------------------------------
        connect(ui->toolButtonMyIdentity , SIGNAL(clicked()), Moneychanger::It(), SLOT(mc_defaultnym_slot()));
        connect(ui->toolButtonMyIdentity2, SIGNAL(clicked()), Moneychanger::It(), SLOT(mc_defaultnym_slot()));
        // --------------------------------------------------------
        connect(this, SIGNAL(showContact (QString)),         Moneychanger::It(), SLOT(mc_show_opentxs_contact_slot(QString)));
        connect(this, SIGNAL(showContacts()),                Moneychanger::It(), SLOT(mc_opentxs_contacts_slot()));
        // --------------------------------------------------------
        connect(this, SIGNAL(showContactAndRefreshHome(QString)), Moneychanger::It(), SLOT(onNeedToPopulateRecordlist()));
        connect(this, SIGNAL(showContactAndRefreshHome(QString)), Moneychanger::It(), SLOT(mc_show_opentxs_contact_slot(QString)));
        // --------------------------------------------------------
        QWidget* pTab0 = ui->tabWidgetTransactions->widget(0);
        QWidget* pTab1 = ui->tabWidgetTransactions->widget(1);

        pTab0->setStyleSheet("QWidget { margin: 0 }");
        pTab1->setStyleSheet("QWidget { margin: 0 }");

//        ui->splitter->setStretchFactor(0, 2);
//        ui->splitter->setStretchFactor(1, 3);

        ui->splitterAccounts->setStretchFactor(0, 1);
        ui->splitterAccounts->setStretchFactor(2, 5);
        // ------------------------
        on_tabWidgetTransactions_currentChanged(0);

        this->on_checkBoxSearchConversations_toggled(false);
        this->on_checkBoxSearchPayments_toggled(false);

        /** Flag Already Init **/
        already_init = true;
    }
    // -------------------------------------------
    RefreshAll();
    // -------------------------------------------
    Focuser f(this);
    f.show();
    f.focus();
    // -------------------------------------------
    setAsCurrentPayment(nSourceRow, nFolder);
}



void Activity::on_tableViewSentSelectionModel_currentRowChanged(const QModelIndex & current, const QModelIndex & previous)
{
    if (!current.isValid())
    {
        disableButtons();
        // ----------------------------------------
        ACTIVITY_TREE_ITEM theItem = make_tree_item(qstrCurrentTLA_, qstrCurrentNotary_, qstrCurrentAccount_, qstrCurrentContact_);
        set_outbox_pmntid_for_tree_item(theItem, 0);
    }
    else
    {
        enableButtons();
        // ----------------------------------------
        QPointer<ModelPayments> pModel = DBHandler::getInstance()->getPaymentModel();

        if (pModel)
        {
            QModelIndex sourceIndex = pPmntProxyModelOutbox_->mapToSource(current);

            QModelIndex haveReadSourceIndex  = pPmntProxyModelOutbox_->sourceModel()->sibling(sourceIndex.row(), PMNT_SOURCE_COL_HAVE_READ, sourceIndex);
            QModelIndex pmntidSourceIndex    = pPmntProxyModelOutbox_->sourceModel()->sibling(sourceIndex.row(), PMNT_SOURCE_COL_PMNT_ID,   sourceIndex);
//            QModelIndex subjectSourceIndex   = pPmntProxyModelOutbox_->sourceModel()->sibling(sourceIndex.row(), PMNT_SOURCE_COL_MEMO,      sourceIndex);
//            QModelIndex senderSourceIndex    = pPmntProxyModelOutbox_->sourceModel()->sibling(sourceIndex.row(), PMNT_SOURCE_COL_MY_NYM,    sourceIndex);
//            QModelIndex recipientSourceIndex = pPmntProxyModelOutbox_->sourceModel()->sibling(sourceIndex.row(), PMNT_SOURCE_COL_RECIP_NYM, sourceIndex);
//            QModelIndex timestampSourceIndex = pPmntProxyModelOutbox_->sourceModel()->sibling(sourceIndex.row(), PMNT_SOURCE_COL_TIMESTAMP, sourceIndex);

//            QModelIndex subjectIndex      = pPmntProxyModelOutbox_->mapFromSource(subjectSourceIndex);
//            QModelIndex senderIndex       = pPmntProxyModelOutbox_->mapFromSource(senderSourceIndex);
//            QModelIndex recipientIndex    = pPmntProxyModelOutbox_->mapFromSource(recipientSourceIndex);
//            QModelIndex timestampIndex    = pPmntProxyModelOutbox_->mapFromSource(timestampSourceIndex);

            QVariant varpmntid    = pModel->data(pmntidSourceIndex);
            QVariant varHaveRead  = pModel->data(haveReadSourceIndex);
//            QVariant varSubject   = pPmntProxyModelOutbox_->data(subjectIndex);
//            QVariant varSender    = pPmntProxyModelOutbox_->data(senderIndex);
//            QVariant varRecipient = pPmntProxyModelOutbox_->data(recipientIndex);
//            QVariant varTimestamp = pPmntProxyModelOutbox_->data(timestampIndex);

//            QString qstrSubject   = varSubject.isValid()   ? varSubject.toString()   : "";
//            QString qstrSender    = varSender.isValid()    ? varSender.toString()    : "";
//            QString qstrRecipient = varRecipient.isValid() ? varRecipient.toString() : "";
//            QString qstrTimestamp = varTimestamp.isValid() ? varTimestamp.toString() : "";
            // ----------------------------------------------------------
            const int payment_id = varpmntid.isValid() ? varpmntid.toInt() : 0;
            if (payment_id > 0)
            {
                ACTIVITY_TREE_ITEM theItem = make_tree_item(qstrCurrentTLA_, qstrCurrentNotary_, qstrCurrentAccount_, qstrCurrentContact_);
                set_outbox_pmntid_for_tree_item(theItem, payment_id);
            }
            // ----------------------------------------------------------
            const bool bHaveRead = varHaveRead.isValid() ? varHaveRead.toBool() : false;

            if (!bHaveRead && (payment_id > 0)) // It's unread, so we need to set it as read.
            {
                listRecordsToMarkAsRead_.append(haveReadSourceIndex);
                QTimer::singleShot(1000, this, SLOT(on_MarkAsRead_timer()));
            }
        }
    }
}

void Activity::on_tableViewReceivedSelectionModel_currentRowChanged(const QModelIndex & current, const QModelIndex & previous)
{
    if (!current.isValid())
    {
        disableButtons();
        // ----------------------------------------
        ACTIVITY_TREE_ITEM theItem = make_tree_item(qstrCurrentTLA_, qstrCurrentNotary_, qstrCurrentAccount_, qstrCurrentContact_);
        set_inbox_pmntid_for_tree_item(theItem, 0);
    }
    else
    {
        enableButtons();
        // ----------------------------------------
        QPointer<ModelPayments> pModel = DBHandler::getInstance()->getPaymentModel();

        if (pModel)
        {
            QModelIndex sourceIndex = pPmntProxyModelInbox_->mapToSource(current);

            QModelIndex haveReadSourceIndex  = pPmntProxyModelInbox_->sourceModel()->sibling(sourceIndex.row(), PMNT_SOURCE_COL_HAVE_READ,  sourceIndex);
            QModelIndex pmntidSourceIndex    = pPmntProxyModelInbox_->sourceModel()->sibling(sourceIndex.row(), PMNT_SOURCE_COL_PMNT_ID,    sourceIndex);
//            QModelIndex subjectSourceIndex   = pPmntProxyModelInbox_->sourceModel()->sibling(sourceIndex.row(), PMNT_SOURCE_COL_MEMO,       sourceIndex);
//            QModelIndex senderSourceIndex    = pPmntProxyModelInbox_->sourceModel()->sibling(sourceIndex.row(), PMNT_SOURCE_COL_SENDER_NYM, sourceIndex);
//            QModelIndex recipientSourceIndex = pPmntProxyModelInbox_->sourceModel()->sibling(sourceIndex.row(), PMNT_SOURCE_COL_MY_NYM,     sourceIndex);
//            QModelIndex timestampSourceIndex = pPmntProxyModelInbox_->sourceModel()->sibling(sourceIndex.row(), PMNT_SOURCE_COL_TIMESTAMP,  sourceIndex);

//            QModelIndex subjectIndex      = pPmntProxyModelInbox_->mapFromSource(subjectSourceIndex);
//            QModelIndex senderIndex       = pPmntProxyModelInbox_->mapFromSource(senderSourceIndex);
//            QModelIndex recipientIndex    = pPmntProxyModelInbox_->mapFromSource(recipientSourceIndex);
//            QModelIndex timestampIndex    = pPmntProxyModelInbox_->mapFromSource(timestampSourceIndex);

            QVariant varpmntid    = pModel->data(pmntidSourceIndex);
            QVariant varHaveRead  = pModel->data(haveReadSourceIndex);
//            QVariant varSubject   = pPmntProxyModelInbox_->data(subjectIndex);
//            QVariant varSender    = pPmntProxyModelInbox_->data(senderIndex);
//            QVariant varRecipient = pPmntProxyModelInbox_->data(recipientIndex);
//            QVariant varTimestamp = pPmntProxyModelInbox_->data(timestampIndex);

//            QString qstrSubject   = varSubject.isValid()   ? varSubject  .toString() : "";
//            QString qstrSender    = varSender   .isValid() ? varSender   .toString() : "";
//            QString qstrRecipient = varRecipient.isValid() ? varRecipient.toString() : "";
//            QString qstrTimestamp = varTimestamp.isValid() ? varTimestamp.toString() : "";

//            ui->headerReceived->setSubject  (qstrSubject);
//            ui->headerReceived->setSender   (qstrSender);
//            ui->headerReceived->setRecipient(qstrRecipient);
//            ui->headerReceived->setTimestamp(qstrTimestamp);
//            ui->headerReceived->setFolder(tr("Received"));

            int payment_id = varpmntid.isValid() ? varpmntid.toInt() : 0;
            if (payment_id > 0)
            {
                ACTIVITY_TREE_ITEM theItem = make_tree_item(qstrCurrentTLA_, qstrCurrentNotary_, qstrCurrentAccount_, qstrCurrentContact_);
                set_inbox_pmntid_for_tree_item(theItem, payment_id);
            }
            // ----------------------------------------------------------
            const bool bHaveRead = varHaveRead.isValid() ? varHaveRead.toBool() : false;

            if (!bHaveRead && (payment_id > 0)) // It's unread, so we need to set it as read.
            {
                listRecordsToMarkAsRead_.append(haveReadSourceIndex);
                QTimer::singleShot(1000, this, SLOT(on_MarkAsRead_timer()));
            }
        }
    }
}

void Activity::on_pushButtonSendMsg_clicked()
{
    const QString qstrPlainText = ui->plainTextEditMsg->toPlainText().simplified();

    if (!qstrPlainText.isEmpty())
    {
        const std::string message{qstrPlainText.toStdString()};

        if (ui->listWidgetConversations->currentRow() <  0)
        {
//            ui->listWidgetConversation->clear();
        }
        else
        {
            QListWidgetItem * conversation_widget = ui->listWidgetConversations->currentItem();

            if (nullptr == conversation_widget) {
                return;
            }

            const QVariant qvarMyNymId  = conversation_widget->data(Qt::UserRole+1);
            const QVariant qvarThreadId = conversation_widget->data(Qt::UserRole+2);

            const QString  qstrMyNymId  = qvarMyNymId.isValid()  ? qvarMyNymId.toString()  : QString("");
            const QString  qstrThreadId = qvarThreadId.isValid() ? qvarThreadId.toString() : QString("");

            if (!qstrMyNymId.isEmpty() && !qstrThreadId.isEmpty())
            {
                const std::string str_my_nym_id = qstrMyNymId.toStdString();
                const std::string str_thread_id = qstrThreadId.toStdString();
                // ----------------------------------------------
                // NOTE: Sometimes the "conversational thread id" is just the contact ID,
                // like when it's just you and the recipient in a private thread.
                // But otherwise, it might be a group conversation, in which case the
                // thread ID is the group ID (and thus NOT a contact ID). But it's okay
                // to pass it either way, because Opentxs handles it properly either way.
                //
                const opentxs::Identifier bgthreadId
                    {opentxs::OT::App().API().OTME_TOO().
                        MessageContact(str_my_nym_id, str_thread_id, message)};

                const auto status = opentxs::OT::App().API().OTME_TOO().Status(bgthreadId);

                const bool bAddToGUI = (opentxs::ThreadStatus::FINISHED_SUCCESS == status) ||
                                       (opentxs::ThreadStatus::RUNNING == status);
                if (bAddToGUI) {
                    const bool bUseGrayText = (opentxs::ThreadStatus::FINISHED_SUCCESS != status);

                    // NOTE: Here is where I could add a gray outgoing item when sending.
                    ui->checkBoxPending->setVisible(true);
                    ui->checkBoxPending->setChecked(true);
                    ui->listWidgetPending->setVisible(true);

                    const int nNewIndex = ui->listWidgetPending->count();
                    // We could "send, then refresh", but instead of refreshing the entire
                    // thread, I'm just adding the item by hand at the end of the list
                    // with gray text. (Or black if it's already finished sending by this
                    // point, which probably will never happen).
                    // Beyond that, I'm now sticking it in a separate control so it doesn't
                    // disappear when the top one gets re-populated.
                    //
                    AddItemToPending(nNewIndex, QString("%1: %2").arg(bUseGrayText ? tr("Queued") : tr("Sent")).arg(qstrPlainText), false, bUseGrayText);
                }
                else {
                    ui->checkBoxPending->setVisible(true);
                    ui->checkBoxPending->setChecked(true);
                    ui->listWidgetPending->setVisible(true);

                    const int nNewIndex = ui->listWidgetPending->count();
                    AddItemToPending(nNewIndex, QString("%1: %2").arg(tr("Permanent failure")).arg(qstrPlainText), false, true);
                }
            }
        }
    }

    ui->plainTextEditMsg->setPlainText("");
}


void Activity::on_checkBoxPending_toggled(bool checked)
{
    if (checked)
    {
        ui->listWidgetPending->setVisible(true);
    }
    else
    {
        ui->listWidgetPending->setVisible(false);
    }
}

bool   Activity::AddItemToPending(int nAtIndex, const QString & qstrContents, bool bIncoming, bool bAddedByHand)
//bool           AddItemToPending(int nAtIndex, const QString & qstrContents, bool bIncoming=false, bool bAddedByHand=true);
{
    if (nullptr == ui->listWidgetPending) {
        return false;
    }

    QListWidgetItem * pItem = nullptr;

    ui->listWidgetPending->addItem(qstrContents);
    pItem = ui->listWidgetPending->item(nAtIndex);

    if (bIncoming)
    {
        pItem->setTextColor(Qt::white);
        pItem->setBackgroundColor(Qt::gray);
        pItem->setTextAlignment(Qt::AlignLeft);
    }
    else
    {
        const auto the_color = bAddedByHand ? Qt::gray : Qt::black;
        pItem->setTextColor(the_color);
        pItem->setBackgroundColor(Qt::white);
        pItem->setTextAlignment(Qt::AlignRight);
    }
    return true;
}


bool Activity::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        if (keyEvent->key() == Qt::Key_Escape)
        {
            close(); // This is caught by this same filter.
            return true;
        }
    }

    // standard event processing
    return QWidget::eventFilter(obj, event);
}

void Activity::on_listWidgetConversations_customContextMenuRequested(const QPoint &pos)
{
//    conversationsPopupMenu(pos, ui->listWidgetConversations);
}



Activity::~Activity()
{
    delete ui;
}



void Activity::on_checkBoxSearchConversations_toggled(bool checked)
{
    if (checked)
    {
        ui->lineEditSearchConversations->setVisible(true);
        ui->pushButtonSearchConversations->setVisible(true);
    }
    else
    {
        ui->lineEditSearchConversations->setVisible(false);
        ui->pushButtonSearchConversations->setVisible(false);
    }
}




void Activity::on_checkBoxSearchPayments_toggled(bool checked)
{
    if (checked)
    {
        ui->lineEditSearchPayments->setVisible(true);
        ui->pushButtonSearchPayments->setVisible(true);

        ui->labelMyIdentity->setVisible(false);
        ui->labelMyIdentity2->setVisible(false);
        ui->toolButtonMyIdentity->setVisible(false);
        ui->toolButtonMyIdentity2->setVisible(false);
    }
    else
    {
        ui->lineEditSearchPayments->setVisible(false);
        ui->pushButtonSearchPayments->setVisible(false);

        ui->lineEditSearchPayments->setText("");

        ui->labelMyIdentity->setVisible(true);
        ui->labelMyIdentity2->setVisible(true);
        ui->toolButtonMyIdentity->setVisible(true);
        ui->toolButtonMyIdentity2->setVisible(true);
    }
}





















void Activity::on_tabWidgetTransactions_currentChanged(int index)
{
    if (ui->tableViewSent      && ui->tableViewReceived &&
        pPmntProxyModelOutbox_ && pPmntProxyModelInbox_)
    {
        pCurrentTabTableView_  = (0 == ui->tabWidgetTransactions->currentIndex()) ? ui->tableViewReceived    : ui->tableViewSent;
        pCurrentTabProxyModel_ = (0 == ui->tabWidgetTransactions->currentIndex()) ? &(*pPmntProxyModelInbox_) : &(*pPmntProxyModelOutbox_);
        // -------------------------------------------------
        QModelIndex the_index  = pCurrentTabTableView_->currentIndex();

        if (the_index.isValid())
            enableButtons();
        else
            disableButtons();
        // --------------------------------------
        RefreshPayments();
    }
    else
    {
        pCurrentTabTableView_  = nullptr;
        pCurrentTabProxyModel_ = nullptr;

        disableButtons();
    }
}


void Activity::enableButtons()
{
//    ui->toolButtonDelete ->setEnabled(true);
//    ui->toolButtonReply  ->setEnabled(true);
//    ui->toolButtonForward->setEnabled(true);
}

void Activity::disableButtons()
{
//    ui->toolButtonDelete ->setEnabled(false);
//    ui->toolButtonReply  ->setEnabled(false);
//    ui->toolButtonForward->setEnabled(false);
}

void Activity::on_MarkAsRead_timer()
{
    QPointer<ModelPayments> pModel = DBHandler::getInstance()->getPaymentModel();

    if (!pModel)
        return;
    // ------------------------------
    bool bEditing = false;

    while (!listRecordsToMarkAsRead_.isEmpty())
    {
        QModelIndex index = listRecordsToMarkAsRead_.front();
        listRecordsToMarkAsRead_.pop_front();
        // ------------------------------------
        if (!index.isValid())
            continue;
        // ------------------------------------
        if (!bEditing)
        {
            bEditing = true;
            pModel->database().transaction();
        }
        // ------------------------------------
        pModel->setData(index, QVariant(1)); // 1 for "true" in sqlite. "Yes, we've now read this payment. Mark it as read."
    } // while
    // ------------------------------
    if (bEditing)
    {
        if (pModel->submitAll())
        {
            pModel->database().commit();
            // ------------------------------------
            QTimer::singleShot(0, this, SLOT(RefreshPayments()));
        }
        else
        {
            pModel->database().rollback();
            qDebug() << "Database Write Error" <<
                       "The database reported an error: " <<
                       pModel->lastError().text();
        }
    }
}

void Activity::on_MarkAsUnread_timer()
{
    QPointer<ModelPayments> pModel = DBHandler::getInstance()->getPaymentModel();

    if (!pModel)
        return;
    // ------------------------------
    bool bEditing = false;

    while (!listRecordsToMarkAsUnread_.isEmpty())
    {
        QModelIndex index = listRecordsToMarkAsUnread_.front();
        listRecordsToMarkAsUnread_.pop_front();
        // ------------------------------------
        if (!index.isValid())
            continue;
        // ------------------------------------
        if (!bEditing)
        {
            bEditing = true;
            pModel->database().transaction();
        }
        // ------------------------------------
        pModel->setData(index, QVariant(0)); // 0 for "false" in sqlite. "This payment is now marked UNREAD."
    } // while
    // ------------------------------
    if (bEditing)
    {
        if (pModel->submitAll())
        {
            pModel->database().commit();
            // ------------------------------------
            QTimer::singleShot(0, this, SLOT(RefreshPayments()));
        }
        else
        {
            pModel->database().rollback();
            qDebug() << "Database Write Error" <<
                       "The database reported an error: " <<
                       pModel->lastError().text();
        }
    }
}

void Activity::on_MarkAsReplied_timer()
{
    QPointer<ModelPayments> pModel = DBHandler::getInstance()->getPaymentModel();

    if (!pModel)
        return;
    // ------------------------------
    bool bEditing = false;

    while (!listRecordsToMarkAsReplied_.isEmpty())
    {
        QModelIndex index = listRecordsToMarkAsReplied_.front();
        listRecordsToMarkAsReplied_.pop_front();
        // ------------------------------------
        if (!index.isValid())
            continue;
        // ------------------------------------
        if (!bEditing)
        {
            bEditing = true;
            pModel->database().transaction();
        }
        // ------------------------------------
        pModel->setData(index, QVariant(1)); // 1 for "true" in sqlite. "Yes, we've now replied to this payment. Mark it as replied."
    } // while
    // ------------------------------
    if (bEditing)
    {
        if (pModel->submitAll())
        {
            pModel->database().commit();
            // ------------------------------------
            QTimer::singleShot(0, this, SLOT(RefreshPayments()));
        }
        else
        {
            pModel->database().rollback();
            qDebug() << "Database Write Error" <<
                       "The database reported an error: " <<
                       pModel->lastError().text();
        }
    }
}

void Activity::on_MarkAsForwarded_timer()
{
    QPointer<ModelPayments> pModel = DBHandler::getInstance()->getPaymentModel();

    if (!pModel)
        return;
    // ------------------------------
    bool bEditing = false;

    while (!listRecordsToMarkAsForwarded_.isEmpty())
    {
        QModelIndex index = listRecordsToMarkAsForwarded_.front();
        listRecordsToMarkAsForwarded_.pop_front();
        // ------------------------------------
        if (!index.isValid())
            continue;
        // ------------------------------------
        if (!bEditing)
        {
            bEditing = true;
            pModel->database().transaction();
        }
        // ------------------------------------
        pModel->setData(index, QVariant(1)); // 1 for "true" in sqlite. "Yes, we've now forwarded this payment. Mark it as forwarded."
    } // while
    // ------------------------------
    if (bEditing)
    {
        if (pModel->submitAll())
        {
            pModel->database().commit();
            // ------------------------------------
            QTimer::singleShot(0, this, SLOT(RefreshPayments()));
        }
        else
        {
            pModel->database().rollback();
            qDebug() << "Database Write Error" <<
                       "The database reported an error: " <<
                       pModel->lastError().text();
        }
    }
}







void Activity::setAsCurrentPayment(int nSourceRow, int nFolder)
{
    if (-1 == nSourceRow || -1 == nFolder)
        return;

    QPointer<ModelPayments> pModel = DBHandler::getInstance()->getPaymentModel();

    if (!pModel)
        return;
    // -------------------
    QTableView * pTableView = (0 == nFolder) ? ui->tableViewSent : ui->tableViewReceived;
    if (nullptr == pTableView) return; // should never happen.
    QPointer<ActivityPaymentsProxyModel> & pProxyModel = (0 == nFolder) ? pPmntProxyModelOutbox_ : pPmntProxyModelInbox_;
    // ----------------------------
    // If the table view we're switching to, is not the current one, then we
    // need to make it the current one.
    //
    if (pTableView != pCurrentTabTableView_)
    {
        ui->tabWidgetTransactions->setCurrentIndex((0 == nFolder) ? 1 : 0);
    }
    // ----------------------------
    QModelIndex sourceIndex = pModel->index(nSourceRow, PMNT_SOURCE_COL_PMNT_ID);
    QModelIndex proxyIndex;
    if (sourceIndex.isValid())
        proxyIndex = pProxyModel->mapFromSource(sourceIndex);
    if (proxyIndex.isValid())
        pTableView->setCurrentIndex(proxyIndex);
}




/*
    QString qstrCurrentTLA_;  // If the user clicks on "BTC" near the top, the current TLA is "BTC".
    QString qstrCurrentNotary_; // If the user clicks on "localhost" (hosted notary) then that Notary ID is set here.
//  QString qstrCurrentWallet_; // If the user clicks on his local BTC wallet, then we set something in here I guess to distinguish it from the local LTC wallet.
    QString qstrCurrentAccount_; // If the user clicks on one of his Opentxs accounts, or a notary with only 1 account under it, we put the acct ID here.
    QString qstrCurrentContact_; // If the user clicks on one of his contacts, we put the Opentxs Contact ID here.

#define ACTIVITY_TREE_HEADER_TOTALS   0
#define ACTIVITY_TREE_HEADER_LOCAL    1
#define ACTIVITY_TREE_HEADER_SNP      2
#define ACTIVITY_TREE_HEADER_HOSTED   3
#define ACTIVITY_TREE_HEADER_CONTACTS 4

#define AC_NODE_TYPE_HEADER          0
#define AC_NODE_TYPE_ASSET_TOTAL     1
#define AC_NODE_TYPE_CONTACT         2
#define AC_NODE_TYPE_LOCAL_WALLET    3
#define AC_NODE_TYPE_LOCAL_ACCOUNT   4
#define AC_NODE_TYPE_SNP_NOTARY      5
#define AC_NODE_TYPE_SNP_ACCOUNT     6
#define AC_NODE_TYPE_HOSTED_NOTARY   7
#define AC_NODE_TYPE_HOSTED_ACCOUNT  8
*/
void Activity::on_treeWidgetAccounts_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    Q_UNUSED(previous);

    if (nullptr != current)
    {
        const int nNodeType = current->data(0, Qt::UserRole  ).toInt();
        const int nHeader   = current->data(0, Qt::UserRole+1).toInt();

        switch (nNodeType) {
            case AC_NODE_TYPE_ASSET_TOTAL: {
                const QVariant qvarTLA = current->data(0, Qt::UserRole+2);

                qstrCurrentTLA_      = qvarTLA.isValid() ? qvarTLA.toString() : QString("");
                qstrCurrentNotary_   = QString("");
                qstrCurrentAccount_  = QString("");
                qstrCurrentContact_  = QString("");

                pPmntProxyModelInbox_ ->setFilterTLA(qstrCurrentTLA_);
                pPmntProxyModelOutbox_->setFilterTLA(qstrCurrentTLA_);
            } break;

            // Todo someday. Currently Moneychanger doesn't have its own local Bitcoin wallet.
            case AC_NODE_TYPE_LOCAL_WALLET:
            case AC_NODE_TYPE_LOCAL_ACCOUNT: {
                qstrCurrentTLA_      = QString("");
                qstrCurrentNotary_   = QString("");
                qstrCurrentAccount_  = QString("");
                qstrCurrentContact_  = QString("");
                // Todo someday.
                pPmntProxyModelInbox_ ->setFilterNone();
                pPmntProxyModelOutbox_->setFilterNone();
            } break;

            case AC_NODE_TYPE_SNP_NOTARY:
            case AC_NODE_TYPE_HOSTED_NOTARY: {
                // If a Notary has a single account, it may display them on a single line.
                // In which case, sometimes we'll want the Notary ID off that line, but
                // sometimes we'll want the account ID. (But the account ID will sometimes
                // not be there, if there are sub-accounts listed under the notary in the UI).
                //
//              const QVariant qvarAccountId = current->data(0, Qt::UserRole+4);
                const QVariant qvarNotaryId  = current->data(0, Qt::UserRole+5);

                qstrCurrentTLA_      = QString("");
                qstrCurrentAccount_  = QString("");
//              qstrCurrentAccount_  = qvarAccountId.isValid() ? qvarAccountId.toString() : QString("");
                qstrCurrentNotary_   = qvarNotaryId.isValid()  ? qvarNotaryId.toString()  : QString("");
                qstrCurrentContact_  = QString("");

                pPmntProxyModelInbox_ ->setFilterNotary(qstrCurrentNotary_);
                pPmntProxyModelOutbox_->setFilterNotary(qstrCurrentNotary_);
            } break;

            case AC_NODE_TYPE_SNP_ACCOUNT:
            case AC_NODE_TYPE_HOSTED_ACCOUNT: {
                const QVariant qvarAccountId = current->data(0, Qt::UserRole+4);

                qstrCurrentTLA_      = QString("");
                qstrCurrentNotary_   = QString("");
                qstrCurrentAccount_  = qvarAccountId.isValid() ? qvarAccountId.toString() : QString("");
                qstrCurrentContact_  = QString("");

                pPmntProxyModelInbox_ ->setFilterAccount(qstrCurrentAccount_);
                pPmntProxyModelOutbox_->setFilterAccount(qstrCurrentAccount_);
            } break;

            case AC_NODE_TYPE_CONTACT: {
                const QVariant qvarContactId = current->data(0, Qt::UserRole+6);

                qstrCurrentTLA_      = QString("");
                qstrCurrentNotary_   = QString("");
                qstrCurrentAccount_  = QString("");
                qstrCurrentContact_  = qvarContactId.isValid() ? qvarContactId.toString() : QString("");

                pPmntProxyModelInbox_ ->setFilterTopLevel(qstrCurrentContact_);
                pPmntProxyModelOutbox_->setFilterTopLevel(qstrCurrentContact_);
            } break;

            default: {
                qstrCurrentTLA_      = QString("");
                qstrCurrentNotary_   = QString("");
                qstrCurrentAccount_  = QString("");
                qstrCurrentContact_  = QString("");

                pPmntProxyModelInbox_ ->setFilterNone();
                pPmntProxyModelOutbox_->setFilterNone();
            } break;
        }
    }
    else
    {
        qstrCurrentTLA_      = QString("");
        qstrCurrentNotary_   = QString("");
        qstrCurrentAccount_  = QString("");
        qstrCurrentContact_  = QString("");

        pPmntProxyModelInbox_ ->setFilterNone();
        pPmntProxyModelOutbox_->setFilterNone();
    }
    // --------------------------------------
    RefreshPayments();
}



void Activity::onClaimsUpdatedForNym(QString nymId)
{
    if (!bRefreshingAfterUpdatedClaims_)
    {
        bRefreshingAfterUpdatedClaims_ = true;
        QTimer::singleShot(500, this, SLOT(RefreshPayments()));
    }
}


void Activity::RefreshPayments()
{
    bRefreshingAfterUpdatedClaims_ = false;
    // -------------------------------------------
    ui->tableViewSent->reset();
    ui->tableViewReceived->reset();
    // -------------------------------------------
    ACTIVITY_TREE_ITEM theItem = make_tree_item(qstrCurrentTLA_, qstrCurrentNotary_, qstrCurrentAccount_, qstrCurrentContact_);

    bool bIsInbox = (0 == ui->tabWidgetTransactions->currentIndex());
    int  nPmntID   = bIsInbox ? get_inbox_pmntid_for_tree_item(theItem) : get_outbox_pmntid_for_tree_item(theItem);

    if (0 == nPmntID) // There's no "current selected payment ID" set for this tree item.
    {
        int nRowToSelect = -1;

        if (pCurrentTabProxyModel_->rowCount() > 0) // But there ARE rows for this tree item...
            nRowToSelect = 0;

        // So let's select the first one in the list!
        QModelIndex previous = pCurrentTabTableView_->currentIndex();
        pCurrentTabTableView_->blockSignals(true);
        pCurrentTabTableView_->selectRow(nRowToSelect);
        pCurrentTabTableView_->blockSignals(false);

        if (bIsInbox)
            on_tableViewReceivedSelectionModel_currentRowChanged(pCurrentTabTableView_->currentIndex(), previous);
        else
            on_tableViewSentSelectionModel_currentRowChanged(pCurrentTabTableView_->currentIndex(), previous);
    }
    else // There IS a "current selected payment ID" for the current tree item.
    {
        // So let's try to select that in the tree again! (If it's still there. Otherwise set it to row 0.)

        QPointer<ModelPayments> pModel = DBHandler::getInstance()->getPaymentModel();

        if (pModel)
        {
            bool bFoundIt = false;

            const int nRowCount = pCurrentTabProxyModel_->rowCount();

            for (int ii = 0; ii < nRowCount; ++ii)
            {
                QModelIndex indexProxy  = pCurrentTabProxyModel_->index(ii, 0);
                QModelIndex indexSource = pCurrentTabProxyModel_->mapToSource(indexProxy);

                QSqlRecord record = pModel->record(indexSource.row());

                if (!record.isEmpty())
                {
                    QVariant the_value = record.value(PMNT_SOURCE_COL_PMNT_ID);
                    const int nRecordpmntid = the_value.isValid() ? the_value.toInt() : 0;

                    if (nRecordpmntid == nPmntID)
                    {
                        bFoundIt = true;

                        QModelIndex previous = pCurrentTabTableView_->currentIndex();
                        pCurrentTabTableView_->blockSignals(true);
                        pCurrentTabTableView_->selectRow(ii);
                        pCurrentTabTableView_->blockSignals(false);

                        if (bIsInbox)
                            on_tableViewReceivedSelectionModel_currentRowChanged(pCurrentTabTableView_->currentIndex(), previous);
                        else
                            on_tableViewSentSelectionModel_currentRowChanged(pCurrentTabTableView_->currentIndex(), previous);
                        break;
                    }
                }
            }
            // ------------------------------------
            if (!bFoundIt)
            {
                int nRowToSelect = -1;

                if (nRowCount > 0)
                    nRowToSelect = 0;

                QModelIndex previous = pCurrentTabTableView_->currentIndex();
                pCurrentTabTableView_->blockSignals(true);
                pCurrentTabTableView_->selectRow(nRowToSelect);
                pCurrentTabTableView_->blockSignals(false);

                if (bIsInbox)
                    on_tableViewReceivedSelectionModel_currentRowChanged(pCurrentTabTableView_->currentIndex(), previous);
                else
                    on_tableViewSentSelectionModel_currentRowChanged(pCurrentTabTableView_->currentIndex(), previous);
            }
        }
    }
    // -------------------------------------------
    ui->tableViewSent->resizeColumnsToContents();
    ui->tableViewSent->resizeRowsToContents();
    ui->tableViewReceived->resizeColumnsToContents();
    ui->tableViewReceived->resizeRowsToContents();
    {
    int nWidthFirstColumn = ui->tableViewSent->columnWidth(0);
    int nNewWidth = static_cast<int>( static_cast<float>(nWidthFirstColumn) * 1.2 );
    ui->tableViewSent->setColumnWidth(0,nNewWidth);
    }{
    int nWidthFirstColumn = ui->tableViewReceived->columnWidth(0);
    int nNewWidth = static_cast<int>( static_cast<float>(nWidthFirstColumn) * 1.2 );
    ui->tableViewReceived->setColumnWidth(0,nNewWidth);
    }
    ui->tableViewSent->horizontalHeader()->setStretchLastSection(true);
    ui->tableViewReceived->horizontalHeader()->setStretchLastSection(true);
}

//void Activity::RefreshTree()
//{
//    ClearTree();
//    // ----------------------------------------
//    ui->treeWidget->blockSignals(true);
//    // ----------------------------------------
//    QList<QTreeWidgetItem *> items;
//    // ------------------------------------
//    QTreeWidgetItem * pUnfilteredItem = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(tr("All Payments (Unfiltered)")));
//    pUnfilteredItem->setData(0, Qt::UserRole, QVariant(0));
//    items.append(pUnfilteredItem);
//    // ----------------------------------------
//    mapIDName mapContacts;

//    if (MTContactHandler::getInstance()->GetContacts(mapContacts))
//    {
//        for (mapIDName::iterator ii = mapContacts.begin(); ii != mapContacts.end(); ii++)
//        {
//            QString qstrContactID   = ii.key();
//            QString qstrContactName = ii.value();
//            int     nContactID      = qstrContactID.toInt();
//            // ------------------------------------
//            QTreeWidgetItem * pTopItem = new QTreeWidgetItem((QTreeWidget *)nullptr, QStringList(qstrContactName));
//            pTopItem->setData(0, Qt::UserRole, QVariant(nContactID));
//            items.append(pTopItem);
//            // ------------------------------------
//            mapIDName mapMethodTypes;
//            // So we can look up these names quickly without having to repeatedly hit the database
//            // for the same names over and over again.
//            MTContactHandler::getInstance()->GetMsgMethodTypes(mapMethodTypes);
//            // ------------------------------------
//            mapIDName mapTransport;

//            if (MTContactHandler::getInstance()->GetMsgMethodTypesByContact(mapTransport, nContactID, true)) // True means to add the OT servers as well.
//            {
//                for (mapIDName::iterator it_transport = mapTransport.begin(); it_transport != mapTransport.end(); it_transport++)
//                {
//                    QString qstrID   = it_transport.key();
////                  QString qstrName = it_transport.value();

//                    QStringList stringlist = qstrID.split("|");

//                    if (stringlist.size() >= 2) // Should always be 2...
//                    {
//                        QString qstrViaTransport = stringlist.at(1);
//                        QString qstrTransportName = qstrViaTransport;
//                        QString qstrMethodType = stringlist.at(0);
//                        QString qstrMethodName = qstrMethodType;

//                        mapIDName::iterator it_mapMethodTypes = mapMethodTypes.find(qstrMethodType);

//                        if (mapMethodTypes.end() != it_mapMethodTypes)
//                        {
//                            QString qstrTemp = it_mapMethodTypes.value();
//                            if (!qstrTemp.isEmpty())
//                                qstrMethodName = qstrTemp;
//                        }
//                        // ------------------------------------------------------
//                        else if (0 == QString("otserver").compare(qstrMethodType))
//                        {
//                            qstrMethodName = tr("Notary");
//                            // ------------------------------
//                            QString qstrTemp = QString::fromStdString(opentxs::OT::App().API().Exec().GetServer_Name(qstrViaTransport.toStdString()));
//                            if (!qstrTemp.isEmpty())
//                                qstrTransportName = qstrTemp;
//                        }
//                        // ------------------------------------------------------
//                        QTreeWidgetItem * pAddressItem = new QTreeWidgetItem(pTopItem, QStringList(qstrContactName) << qstrMethodName << qstrTransportName);
//                        pAddressItem->setData(0, Qt::UserRole, QVariant(nContactID));
//                        pAddressItem->setData(1, Qt::UserRole, QVariant(qstrMethodType));
//                        pAddressItem->setData(2, Qt::UserRole, QVariant(qstrViaTransport));
//                        items.append(pAddressItem);
//                    }
//                }
//            }
//        }
//        if (items.count() > 0)
//        {
//            ui->treeWidget->insertTopLevelItems(0, items);
//            ui->treeWidget->resizeColumnToContents(0);
//        }
//    }
//    // ----------------------------------------
//    // Make sure the same item that was selected before, is selected again.
//    // (If it still exists, which it probably does.)

//    QTreeWidgetItem * previous = ui->treeWidget->currentItem();

//    // In this case, just select the first thing on the list.
//    if ( (qstrCurrentContact_.isEmpty()) && qstrMethodType_.isEmpty() && qstrViaTransport_.isEmpty() )
//    {
//        if (ui->treeWidget->topLevelItemCount() > 0)
//            ui->treeWidget->setCurrentItem(ui->treeWidget->topLevelItem(0));
//    }
//    else // Find the one that was selected before the refresh.
//    {
//        bool bFoundIt = false;

//        QTreeWidgetItemIterator it(ui->treeWidget);

//        while (*it)
//        {
//            QVariant qvarContactID    = (*it)->data(0, Qt::UserRole);
//            QVariant qvarMethodType   = (*it)->data(1, Qt::UserRole);
//            QVariant qvarViaTransport = (*it)->data(2, Qt::UserRole);

//            const int     nContactID       = qvarContactID   .isValid() ? qvarContactID   .toInt()    :  0;
//            const QString qstrMethodType   = qvarMethodType  .isValid() ? qvarMethodType  .toString() : "";
//            const QString qstrViaTransport = qvarViaTransport.isValid() ? qvarViaTransport.toString() : "";

//            if ( (nContactID == nCurrentContact_ ) &&
//                 (0 == qstrMethodType.compare(qstrMethodType_)) &&
//                 (0 == qstrViaTransport.compare(qstrViaTransport_)) )
//            {
//                bFoundIt = true;
//                ui->treeWidget->setCurrentItem(*it);
//                break;
//            }
//            // ------------------------
//            ++it;
//        } //while

//        if (!bFoundIt)
//        {
//            if (ui->treeWidget->topLevelItemCount() > 0)
//                ui->treeWidget->setCurrentItem(ui->treeWidget->topLevelItem(0));
//        }
//    }
//    // ----------------------------------------
//    ui->treeWidget->blockSignals(false);
//    // ----------------------------------------
//    on_treeWidget_currentItemChanged(ui->treeWidget->currentItem(), previous);
//}

// --------------------------------------------------

void Activity::on_tableViewReceived_customContextMenuRequested(const QPoint &pos)
{
    tableViewPayments_PopupMenu(pos, ui->tableViewReceived, &(*pPmntProxyModelInbox_));
}

void Activity::on_tableViewSent_customContextMenuRequested(const QPoint &pos)
{
    tableViewPayments_PopupMenu(pos, ui->tableViewSent, &(*pPmntProxyModelOutbox_));
}

// --------------------------------------------------
// TODO resume: payments::AcceptIncoming, CancelOutgoing, DiscardOutgoingCash, and DiscardIncoming:

void Activity::AcceptIncoming(QPointer<ModelPayments> & pModel, ActivityPaymentsProxyModel * pProxyModel, const int nSourceRow, QTableView * pTableView)
{
    emit showDashboard();
}

void Activity::CancelOutgoing(QPointer<ModelPayments> & pModel, ActivityPaymentsProxyModel * pProxyModel, const int nSourceRow, QTableView * pTableView)
{
    emit showDashboard();
}

void Activity::DiscardOutgoingCash(QPointer<ModelPayments> & pModel, ActivityPaymentsProxyModel * pProxyModel, const int nSourceRow, QTableView * pTableView)
{
    emit showDashboard();
}

void Activity::DiscardIncoming(QPointer<ModelPayments> & pModel, ActivityPaymentsProxyModel * pProxyModel, const int nSourceRow, QTableView * pTableView)
{
    emit showDashboard();
}
// --------------------------------------------------

void Activity::tableViewPayments_PopupMenu(const QPoint &pos, QTableView * pTableView, ActivityPaymentsProxyModel * pProxyModel)
{
    QPointer<ModelPayments> pModel = DBHandler::getInstance()->getPaymentModel();

    if (!pModel)
        return;
    // ------------------------
    QModelIndex indexAtRightClick = pTableView->indexAt(pos);
    if (!indexAtRightClick.isValid())
        return;
    // I can't figure out how to ADD to the selection without UNSELECTING everything else.
    // The Qt docs indicate that the below options should do that -- but it doesn't work.
    // So this is commented out since it was deselecting everything.
    //pTableView->selectionModel()->select( indexAtRightClick, QItemSelectionModel::SelectCurrent|QItemSelectionModel::Rows );
    // ------------------------
    QModelIndex sourceIndexAtRightClick = pProxyModel->mapToSource(indexAtRightClick);
    const int nRow = sourceIndexAtRightClick.row();
    // ----------------------------------
    resetPopupMenus();
    popupMenuPayments_.reset(new QMenu(this));

    pActionOpenNewWindow = popupMenuPayments_->addAction(tr("View instrument(s)"));
    pActionReply = popupMenuPayments_->addAction(tr("Reply"));
    pActionForward = popupMenuPayments_->addAction(tr("Forward"));
    popupMenuPayments_->addSeparator();
    pActionDelete = popupMenuPayments_->addAction(tr("Delete"));
    popupMenuPayments_->addSeparator();
    pActionMarkRead = popupMenuPayments_->addAction(tr("Mark as read"));
    pActionMarkUnread = popupMenuPayments_->addAction(tr("Mark as unread"));
    // ----------------------------------
    QString qstrContactId("");

    QString qstrSenderNymId;
    QString qstrSenderAddr;
    QString qstrRecipientNymId;
    QString qstrRecipientAddr;
    QString qstrNotaryId;
    QString qstrMethodType;
//  QString qstrSubject;

    // new-style
    QString qstrSenderContactByNym("");
    QString qstrRecipientContactByNym("");
    QString qstrSenderContactByAddress("");
    QString qstrRecipientContactByAddress("");

    ModelPayments::PaymentFlags flags = ModelPayments::NoFlags;
    // ----------------------------------------------
    // Look at the data for indexAtRightClick and see if I have a contact already in the
    // address book. If so, add the "View Contact" option to the menu. But if not, add the
    // "Create Contact" and "Add to Existing Contact" options to the menu instead.
    //
    // UPDATE: I've now also added similar functionality, for other actions specific
    // to certain payment records, based on their flags. (Pay this invoice, deposit
    // this cash, etc.)
    //
    if (nRow >= 0)
    {
        QModelIndex indexSenderNym     = pModel->index(nRow, PMNT_SOURCE_COL_SENDER_NYM);
        QModelIndex indexSenderAddr    = pModel->index(nRow, PMNT_SOURCE_COL_SENDER_ADDR);
        QModelIndex indexRecipientNym  = pModel->index(nRow, PMNT_SOURCE_COL_RECIP_NYM);
        QModelIndex indexRecipientAddr = pModel->index(nRow, PMNT_SOURCE_COL_RECIP_ADDR);
        QModelIndex indexNotaryId      = pModel->index(nRow, PMNT_SOURCE_COL_NOTARY_ID);
        QModelIndex indexMethodType    = pModel->index(nRow, PMNT_SOURCE_COL_METHOD_TYPE);
        QModelIndex indexFlags         = pModel->index(nRow, PMNT_SOURCE_COL_FLAGS);
//      QModelIndex indexSubject       = pModel->index(nRow, PMNT_SOURCE_COL_MEMO);

        QVariant varSenderNym     = pModel->rawData(indexSenderNym);
        QVariant varSenderAddr    = pModel->rawData(indexSenderAddr);
        QVariant varRecipientNym  = pModel->rawData(indexRecipientNym);
        QVariant varRecipientAddr = pModel->rawData(indexRecipientAddr);
        QVariant varNotaryId      = pModel->rawData(indexNotaryId);
        QVariant varMethodType    = pModel->rawData(indexMethodType);
        QVariant varFlags         = pModel->rawData(indexFlags);
//      QVariant varSubject       = pModel->rawData(indexSubject);

        qint64 lFlags      = varFlags        .isValid() ? varFlags        .toLongLong() : 0;
        qstrSenderNymId    = varSenderNym    .isValid() ? varSenderNym    .toString()   : QString("");
        qstrSenderAddr     = varSenderAddr   .isValid() ? varSenderAddr   .toString()   : QString("");
        qstrRecipientNymId = varRecipientNym .isValid() ? varRecipientNym .toString()   : QString("");
        qstrRecipientAddr  = varRecipientAddr.isValid() ? varRecipientAddr.toString()   : QString("");
        qstrNotaryId       = varNotaryId     .isValid() ? varNotaryId     .toString()   : QString("");
        qstrMethodType     = varMethodType   .isValid() ? varMethodType   .toString()   : QString("");
//      qstrSubject        = varSubject      .isValid() ? varSubject      .toString()   : QString("");

        // new-style
        const opentxs::Identifier senderContactId    = opentxs::OT::App().Contact().ContactID(opentxs::Identifier{qstrSenderNymId.toStdString()});
        const opentxs::Identifier recipientContactId = opentxs::OT::App().Contact().ContactID(opentxs::Identifier{qstrRecipientNymId.toStdString()});
        const opentxs::String     strSenderContactId(senderContactId);
        const opentxs::String     strRecipientContactId(recipientContactId);

        qstrSenderContactByNym     = senderContactId.empty()    ? "" : QString::fromStdString(std::string(strSenderContactId.Get()));
        qstrRecipientContactByNym  = recipientContactId.empty() ? "" : QString::fromStdString(std::string(strRecipientContactId.Get()));

        qstrContactId = (!qstrSenderContactByNym.isEmpty()) ? qstrSenderContactByNym : qstrRecipientContactByNym;

        flags = ModelPayments::PaymentFlag(static_cast<ModelPayments::PaymentFlag>(lFlags));
        // -------------------------------
        popupMenuPayments_->addSeparator();
        // -------------------------------
        if (!qstrContactId.isEmpty() ) // There's a known contact for this payment.
            pActionViewContact = popupMenuPayments_->addAction(tr("View contact in address book"));
        else // There is no known contact for this payment.
        {
            pActionCreateContact = popupMenuPayments_->addAction(tr("Create new contact"));
            pActionExistingContact = popupMenuPayments_->addAction(tr("Add to existing contact"));
        }
        // -------------------------------
        popupMenuPayments_->addSeparator();
        // -------------------------------
        pActionDownloadCredentials = popupMenuPayments_->addAction(tr("Download credentials"));
        // -------------------------------
        popupMenuPayments_->addSeparator();
        // -------------------------------
        if ( flags.testFlag(ModelPayments::CanAcceptIncoming))
        {
            QString nameString;
            QString actionString;

            if ( flags.testFlag(ModelPayments::IsTransfer) )
            {
                nameString = tr("Accept this Transfer");
                actionString = tr("Accepting...");
            }
            else if ( flags.testFlag(ModelPayments::IsReceipt) )
            {
                nameString = tr("Accept this Receipt");
                actionString = tr("Accepting...");
            }
            else if ( flags.testFlag(ModelPayments::IsInvoice) )
            {
                nameString = tr("Pay this Invoice");
                actionString = tr("Paying...");
            }
            else if ( flags.testFlag(ModelPayments::IsPaymentPlan) )
            {
                nameString = tr("Activate this Payment Plan");
                actionString = tr("Activating...");
            }
            else if ( flags.testFlag(ModelPayments::IsContract) )
            {
                nameString = tr("Sign this Smart Contract");
                actionString = tr("Signing...");
            }
            else if ( flags.testFlag(ModelPayments::IsCash) )
            {
                nameString = tr("Deposit this Cash");
                actionString = tr("Depositing...");
            }
            else if ( flags.testFlag(ModelPayments::IsCheque) )
            {
                nameString = tr("Deposit this Cheque");
                actionString = tr("Depositing...");
            }
            else if ( flags.testFlag(ModelPayments::IsVoucher) )
            {
                nameString = tr("Accept this Payment");
                actionString = tr("Accepting...");
            }
            else
            {
                nameString = tr("Deposit this Payment");
                actionString = tr("Depositing...");
            }

            pActionAcceptIncoming = popupMenuPayments_->addAction(nameString);
        }

        if (flags.testFlag(ModelPayments::CanCancelOutgoing))
        {
            QString cancelString;
            QString actionString = tr("Canceling...");
//          QString msg = tr("Cancellation Failed. Perhaps recipient had already accepted it?");

            if (flags.testFlag(ModelPayments::IsInvoice))
                cancelString = tr("Cancel this Invoice");
            else if (flags.testFlag(ModelPayments::IsPaymentPlan))
                cancelString = tr("Cancel this Payment Plan");
            else if (flags.testFlag(ModelPayments::IsContract))
                cancelString = tr("Cancel this Smart Contract");
            else if (flags.testFlag(ModelPayments::IsCash))
            {
                cancelString = tr("Recover this Cash");
                actionString = tr("Recovering...");
//              msg = tr("Recovery Failed. Perhaps recipient had already accepted it?");
            }
            else if (flags.testFlag(ModelPayments::IsCheque))
                cancelString = tr("Cancel this Cheque");
            else if (flags.testFlag(ModelPayments::IsVoucher))
                cancelString = tr("Cancel this Payment");
            else
                cancelString = tr("Cancel this Payment");

            pActionCancelOutgoing = popupMenuPayments_->addAction(cancelString);
        }

        if (flags.testFlag(ModelPayments::CanDiscardOutgoingCash))
        {
            QString discardString = tr("Discard this Sent Cash");

            pActionDiscardOutgoingCash = popupMenuPayments_->addAction(discardString);
        }

        if (flags.testFlag(ModelPayments::CanDiscardIncoming))
        {
            QString discardString;

            if (flags.testFlag(ModelPayments::IsInvoice))
                discardString = tr("Discard this Invoice");
            else if (flags.testFlag(ModelPayments::IsPaymentPlan))
                discardString = tr("Discard this Payment Plan");
            else if (flags.testFlag(ModelPayments::IsContract))
                discardString = tr("Discard this Smart Contract");
            else if (flags.testFlag(ModelPayments::IsCash))
                discardString = tr("Discard this Cash");
            else if (flags.testFlag(ModelPayments::IsCheque))
                discardString = tr("Discard this Cheque");
            else if (flags.testFlag(ModelPayments::IsVoucher))
                discardString = tr("Discard this Payment");
            else
                discardString = tr("Discard this Payment");

            pActionDiscardIncoming = popupMenuPayments_->addAction(discardString);
        }
    }
    // --------------------------------------------------
    QPoint globalPos = pTableView->mapToGlobal(pos);
    const QAction* selectedAction = popupMenuPayments_->exec(globalPos); // Here we popup the menu, and get the user's click.
    if (nullptr == selectedAction)
        return;
    // ----------------------------------
    if (selectedAction == pActionAcceptIncoming) // Only approves the current payment.
    {
        pTableView->setCurrentIndex(indexAtRightClick);
        AcceptIncoming(pModel, pProxyModel, nRow, pTableView);
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionCancelOutgoing) // Only cancels the current payment.
    {
        pTableView->setCurrentIndex(indexAtRightClick);
        CancelOutgoing(pModel, pProxyModel, nRow, pTableView);
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionDiscardOutgoingCash) // Only discards the current payment.
    {
        pTableView->setCurrentIndex(indexAtRightClick);
        DiscardOutgoingCash(pModel, pProxyModel, nRow, pTableView);
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionDiscardIncoming) // Only discards the current payment.
    {
        pTableView->setCurrentIndex(indexAtRightClick);
        DiscardIncoming(pModel, pProxyModel, nRow, pTableView);
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionReply) // Only replies to the current payment.
    {
        pTableView->setCurrentIndex(indexAtRightClick);
        on_toolButtonReply_clicked();
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionForward) // Only fowards the current payments.
    {
        pTableView->setCurrentIndex(indexAtRightClick);
        on_toolButtonForward_clicked();
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionDelete) // May delete many payments.
    {
        on_toolButtonDelete_clicked();
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionOpenNewWindow) // May open many payments.
    {
        pTableView->setCurrentIndex(indexAtRightClick);

        if (pTableView == ui->tableViewReceived)
            on_tableViewReceived_doubleClicked(indexAtRightClick); // just one for now. baby steps!
        else if (pTableView == ui->tableViewSent)
            on_tableViewSent_doubleClicked(indexAtRightClick); // just one for now. baby steps!
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionMarkRead) // May mark many payments.
    {
        if (!pTableView->selectionModel()->hasSelection())
            return;
        // ----------------------------------------------
        QItemSelection selection( pTableView->selectionModel()->selection() );
        QList<int> rows;
        foreach( const QModelIndex & index, selection.indexes() )
        {
            if (rows.indexOf(index.row()) != (-1)) // This row is already on the list, so skip it.
                continue;
            rows.append(index.row());
            // -----------------------
            QModelIndex sourceIndex = pProxyModel->mapToSource(index);
            QModelIndex sourceIndexHaveRead = pModel->sibling(sourceIndex.row(), PMNT_SOURCE_COL_HAVE_READ, sourceIndex);
            // --------------------------------
            if (sourceIndexHaveRead.isValid())
                listRecordsToMarkAsRead_.append(sourceIndexHaveRead);
        }
        if (listRecordsToMarkAsRead_.count() > 0)
            QTimer::singleShot(0, this, SLOT(on_MarkAsRead_timer()));
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionMarkUnread) // May mark many payments.
    {
        if (!pTableView->selectionModel()->hasSelection())
            return;
        // ----------------------------------------------
        QItemSelection selection( pTableView->selectionModel()->selection() );
        QList<int> rows;
        foreach( const QModelIndex & index, selection.indexes() )
        {
            if (rows.indexOf(index.row()) != (-1)) // This row is already on the list, so skip it.
                continue;
            rows.append(index.row());
            // -----------------------
            QModelIndex sourceIndex = pProxyModel->mapToSource(index);
            QModelIndex sourceIndexHaveRead = pModel->sibling(sourceIndex.row(), PMNT_SOURCE_COL_HAVE_READ, sourceIndex);
            // --------------------------------
            if (sourceIndexHaveRead.isValid())
                listRecordsToMarkAsUnread_.append(sourceIndexHaveRead);
        }
        if (listRecordsToMarkAsUnread_.count() > 0)
            QTimer::singleShot(0, this, SLOT(on_MarkAsUnread_timer()));
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionViewContact)
    {
        pTableView->setCurrentIndex(indexAtRightClick);

        if (!qstrContactId.isEmpty())
        {
            emit showContact(qstrContactId);
        }
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionCreateContact)
    {
        pTableView->setCurrentIndex(indexAtRightClick);

        MTGetStringDialog nameDlg(this, tr("Enter a name for the new contact"));

        if (QDialog::Accepted != nameDlg.exec())
            return;
        // --------------------------------------
        QString strNewContactName = nameDlg.GetOutputString();
        // --------------------------------------------------
        // NOTE:
        // if nSenderContactByNym > 0, then the sender Nym already has a contact.
        // else if nSenderContactByNym == 0 but qstrSenderNymId exists, that means it
        // contains a NymID that could be added to an existing contact, or used to
        // create a new contact. (And the same is true for the Sender Address.)
        //
        // (And the same is also true for the recipient nymID and address.)
        //
        if (qstrSenderContactByNym.isEmpty() && !qstrSenderNymId.isEmpty())
            qstrContactId = MTContactHandler::getInstance()->GetOrCreateOpentxsContactBasedOnNym(strNewContactName, qstrSenderNymId);
        else if (qstrRecipientContactByNym.isEmpty() && !qstrRecipientNymId.isEmpty())
            qstrContactId = MTContactHandler::getInstance()->GetOrCreateOpentxsContactBasedOnNym(strNewContactName, qstrRecipientNymId);
        // -----------------------------------------------------
        if (!qstrContactId.isEmpty())
        {
            emit showContactAndRefreshHome(qstrContactId);
        }
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionDownloadCredentials)
    {
        pTableView->setCurrentIndex(indexAtRightClick);

        const bool bHaveContact = !qstrContactId.isEmpty();
        mapIDName mapNymIds;

        if (bHaveContact)
        {
            MTContactHandler::getInstance()->GetNyms(mapNymIds, qstrContactId.toStdString());

            // Check to see if there is more than one Nym for this contact.
            // TODO: If so, get the user to select one of the Nyms, or give him the
            // option to do them all.
            // (Until then, we're just going to do them all.)
        }
        // ---------------------------------------------------
        QString qstrNymId;

        if      (!qstrSenderNymId.isEmpty())    qstrNymId   = qstrSenderNymId;
        else if (!qstrRecipientNymId.isEmpty()) qstrNymId   = qstrRecipientNymId;
        // ---------------------------------------------------
        // Might not have a contact. Even if we did, he might not have any NymIds.
        // Here, if there are no known NymIds, but there's one on the message,
        // then we add it to the map.
        if ( (0 == mapNymIds.size()) && (qstrNymId.size() > 0) )
        {
            mapNymIds.insert(qstrNymId, QString("Name not used here"));
        }
        // ---------------------------------------------------
        if (0 == mapNymIds.size())
        {
            QMessageBox::warning(this, tr("Moneychanger"), tr("Unable to find a NymId for this message. (Unable to download credentials without Id.)"));
            qDebug() << "Unable to find a NymId for this message. (Unable to download credentials without Id.)";
            return;
        }
        // Below this point we're guaranteed that there's at least one NymID.
        // ---------------------------------------------------
        int nFound = 0;
        for (mapIDName::iterator
             it_nyms  = mapNymIds.begin();
             it_nyms != mapNymIds.end();
             ++it_nyms)
        {
            nFound++;
            emit needToCheckNym("", it_nyms.key(), qstrNotaryId);
        }
    }
    // ----------------------------------
    else if (selectedAction == pActionExistingContact)
    {
        pTableView->setCurrentIndex(indexAtRightClick);

        // This should never happen since we wouldn't even have gotten this menu option
        // in the first place, unless contact ID had been 0.
        if (!qstrContactId.isEmpty())
            return;

        // (And that means no contact was found for ANY of the Nym IDs or Addresses on this payment.)
        // That means we can add the first one we find (which will probably be the only one as well.)
        // Because I'll EITHER have a SenderNymID OR SenderAddress,
        // ...OR I'll have a RecipientNymID OR RecipientAddress.
        // Thus, only one of the four IDs/Addresses will actually be found.
        // Therefore I don't care which one I find first:
        //
        QString qstrNymId;

        if      (!qstrSenderNymId.isEmpty())    qstrNymId   = qstrSenderNymId;
        else if (!qstrRecipientNymId.isEmpty()) qstrNymId   = qstrRecipientNymId;
        // ---------------------------------------------------
        if (qstrNymId.isEmpty()) // Should never happen.
            return;
        // Below this point we're guaranteed that there's a NymID.
        // ---------------------------------------------------
        const opentxs::Identifier contactId    = opentxs::OT::App().Contact().ContactID(opentxs::Identifier{qstrNymId.toStdString()});

        if (!contactId.empty())
        {
            QMessageBox::warning(this, tr("Moneychanger"),
                                 tr("Strange: NymID %1 already belongs to an existing contact.").arg(qstrNymId));
            return;
        }
        // ---------------------------------------------------
        // Pop up a Contact selection box. The user chooses an existing contact.
        // If OK (vs Cancel) then add the Nym / Acct to the existing contact selected.
        //
        DlgChooser theChooser(this);
        // -----------------------------------------------
        mapIDName & the_map = theChooser.m_map;
        MTContactHandler::getInstance()->GetOpentxsContacts(the_map);
        // -----------------------------------------------
        theChooser.setWindowTitle(tr("Choose an Existing Contact"));
        if (theChooser.exec() != QDialog::Accepted)
            return;
        // -----------------------------------------------
        QString strContactID = theChooser.GetCurrentID();

        if (!strContactID.isEmpty())
        {
            if (!qstrNymId.isEmpty()) // We're adding this NymID to the contact.
            {
                const bool bAdded = false;
                if (!bAdded) // Todo.
                    /*
                     * Justus:
                       - Identifier ContactManager::ContactID(const Identifier& nymID) const
                         That will tell you if a nym is associated with a contact

                       - bool Contact::AddNym(const Identifier& nymID, const bool primary);
                         Will add it to an existing contact
                    */

//              if (!MTContactHandler::getInstance()->AddNymToExistingContact(nContactId, qstrNymId))
                {
                    QMessageBox::warning(this, tr("Moneychanger"),
                                         tr("TODO: Tried to add NymID %1 to an existing contact but I don't know the API call to use.").arg(qstrNymId));
                    return;
                }
                else if (!qstrNotaryId.isEmpty())
                    MTContactHandler::getInstance()->NotifyOfNymServerPair(qstrNymId, qstrNotaryId);
            }
            // ---------------------------------
            // Display the normal contacts dialog, with the new contact
            // being the one selected.
            //
            emit showContactAndRefreshHome(strContactID);
            // ---------------------------------
        } // (!strContactID.isEmpty())
    }
}


void Activity::on_tableViewReceived_doubleClicked(const QModelIndex &index)
{
    tableViewPayments_DoubleClicked(index, &(*pPmntProxyModelInbox_));
}

void Activity::on_tableViewSent_doubleClicked(const QModelIndex &index)
{
    tableViewPayments_DoubleClicked(index, &(*pPmntProxyModelOutbox_));
}

void Activity::tableViewPayments_DoubleClicked(const QModelIndex &index, ActivityPaymentsProxyModel * pProxyModel)
{
    QPointer<ModelPayments> pModel = DBHandler::getInstance()->getPaymentModel();

    if (!pModel)
        return;

    if (!index.isValid())
        return;

    QModelIndex sourceIndex = pProxyModel->mapToSource(index);

    if (!sourceIndex.isValid())
        return;
    // -------------------------------
    QModelIndex pmntidIndex   = pModel->sibling(sourceIndex.row(), PMNT_SOURCE_COL_PMNT_ID, sourceIndex);
    QModelIndex subjectIndex = pModel->sibling(sourceIndex.row(), PMNT_SOURCE_COL_MEMO, sourceIndex);

    QVariant qvarpmntid   = pModel->data(pmntidIndex);
    QVariant qvarSubject = pModel->data(subjectIndex);

    int     nPaymentID  = qvarpmntid.isValid() ? qvarpmntid.toInt() : 0;
    QString qstrSubject = qvarSubject.isValid() ? qvarSubject.toString() : "";
    // -------------------------------
    QString qstrPayment, qstrPending, qstrType, qstrSubtitle;
    // --------------------------------------------------
    if (nPaymentID > 0)
    {
        qstrPayment = MTContactHandler::getInstance()->GetPaymentBody(nPaymentID);
        qstrPending = MTContactHandler::getInstance()->GetPaymentPendingBody(nPaymentID);
    }
    // --------------------------------------------------
    QModelIndex myNymIndex        = pModel->sibling(sourceIndex.row(), PMNT_SOURCE_COL_MY_NYM, sourceIndex);
    QModelIndex senderNymIndex    = pModel->sibling(sourceIndex.row(), PMNT_SOURCE_COL_SENDER_NYM, sourceIndex);
    QModelIndex recipientNymIndex = pModel->sibling(sourceIndex.row(), PMNT_SOURCE_COL_RECIP_NYM, sourceIndex);

    QModelIndex myNymProxyIndex        = pProxyModel->mapFromSource(myNymIndex);
    QModelIndex senderNymProxyIndex    = pProxyModel->mapFromSource(senderNymIndex);
    QModelIndex recipientNymProxyIndex = pProxyModel->mapFromSource(recipientNymIndex);

    QVariant qvarMyNymName        = myNymProxyIndex.isValid()        ? pProxyModel->data(myNymProxyIndex) : QString("");
    QVariant qvarSenderNymName    = senderNymProxyIndex.isValid()    ? pProxyModel->data(senderNymProxyIndex) : QString("");
    QVariant qvarRecipientNymName = recipientNymProxyIndex.isValid() ? pProxyModel->data(recipientNymProxyIndex) : QString("");

    QString qstrMyNymName        = qvarMyNymName.isValid() ? qvarMyNymName.toString() : "";
    QString qstrSenderNymName    = qvarSenderNymName.isValid() ? qvarSenderNymName.toString() : "";
    QString qstrRecipientNymName = qvarRecipientNymName.isValid() ? qvarRecipientNymName.toString() : "";

    if (!qstrSenderNymName.isEmpty())
    {
        qstrType = QString("%1: %2").arg(tr("To")).arg(qstrMyNymName);
        qstrSubtitle = QString("%1: %2").arg(tr("From")).arg(qstrSenderNymName);
    }
    else if (!qstrRecipientNymName.isEmpty())
    {
        qstrType = QString("%1: %2").arg(tr("To")).arg(qstrRecipientNymName);
        qstrSubtitle = QString("%1: %2").arg(tr("From")).arg(qstrMyNymName);
    }
    else
    {
        qstrType = QString("Instrument:");
        qstrSubtitle = QString(" ");
    }
    // -----------
    // Pop up the result dialog.
    //
    if (qstrPayment.isEmpty() || qstrPending.isEmpty())
    {
        DlgExportedToPass dlgExported(this, qstrPayment.isEmpty() ? qstrPending : qstrPayment,
                                      qstrType,
                                      qstrSubtitle, false);
        dlgExported.setWindowTitle(QString("%1: %2").arg(tr("Memo")).arg(qstrSubject));
        dlgExported.exec();
    }
    else
    {
        DlgExportedCash dlgExported(this, qstrPending, qstrPayment,
                                    tr("Receipt:"), QString(" "),
                                    qstrType,
                                    qstrSubtitle, false);
        dlgExported.setWindowTitle(QString("%1: %2").arg(tr("Memo")).arg(qstrSubject));
        dlgExported.exec();
    }
}


void Activity::on_toolButtonReply_clicked()
{
    QPointer<ModelPayments> pModel = DBHandler::getInstance()->getPaymentModel();

    if (!pModel)
        return;
    // ------------------------------------
    QModelIndex proxyIndex = pCurrentTabTableView_->currentIndex();

    if (!proxyIndex.isValid())
        return;
    // ------------------------------------
    QModelIndex sourceIndex = pCurrentTabProxyModel_->mapToSource(proxyIndex);

    if (!sourceIndex.isValid())
        return;

    QModelIndex haveRepliedIndex = pModel->sibling(sourceIndex.row(),
                                                   PMNT_SOURCE_COL_HAVE_REPLIED,
                                                   sourceIndex);
    // ------------------------------------
    QSqlRecord record = pModel->record(sourceIndex.row());

    if (record.isEmpty())
        return;
    // ------------------------------------
    const int nPaymentID = record.value(PMNT_SOURCE_COL_PMNT_ID).isValid() ? record.value(PMNT_SOURCE_COL_PMNT_ID).toInt() : 0;

    const bool bOutgoing = (0 == record.value(PMNT_SOURCE_COL_FOLDER).toInt());

    const QVariant qvar_method_type = record.value(PMNT_SOURCE_COL_METHOD_TYPE);
    const QString  methodType = qvar_method_type.isValid() ? qvar_method_type.toString() : "";

    const QVariant qvar_my_nym_id = record.value(PMNT_SOURCE_COL_MY_NYM);
    const QString  myNymID = qvar_my_nym_id.isValid() ? qvar_my_nym_id.toString() : "";

    const QVariant qvar_my_addr = record.value(PMNT_SOURCE_COL_MY_ADDR);
    const QString  myAddress = qvar_my_addr.isValid() ? qvar_my_addr.toString() : "";

    const QVariant qvar_sender_nym_id = record.value(PMNT_SOURCE_COL_SENDER_NYM);
    const QString  senderNymID = qvar_sender_nym_id.isValid() ? qvar_sender_nym_id.toString() : "";

    const QVariant qvar_recipient_nym_id = record.value(PMNT_SOURCE_COL_RECIP_NYM);
    const QString  recipientNymID = qvar_recipient_nym_id.isValid() ? qvar_recipient_nym_id.toString() : "";

    const QVariant qvar_notary_id = record.value(PMNT_SOURCE_COL_NOTARY_ID);
    const QString  NotaryID = qvar_notary_id.isValid() ? qvar_notary_id.toString() : "";

    const QVariant qvar_sender_addr = record.value(PMNT_SOURCE_COL_SENDER_ADDR);
    const QString  senderAddr = qvar_sender_addr.isValid() ? qvar_sender_addr.toString() : "";

    const QVariant qvar_recipient_addr = record.value(PMNT_SOURCE_COL_RECIP_ADDR);
    const QString  recipientAddr = qvar_recipient_addr.isValid() ? qvar_recipient_addr.toString() : "";

    const QVariant qvar_subject = record.value(PMNT_SOURCE_COL_MEMO);
    const QString  subject = qvar_subject.isValid() ? MTContactHandler::getInstance()->Decode(qvar_subject.toString()) : "";
    // --------------------------------------------------
    const QString& otherNymID = bOutgoing ? recipientNymID : senderNymID;
    const QString& otherAddress  = bOutgoing ? recipientAddr  : senderAddr;
    // --------------------------------------------------
    const bool bUsingNotary   = !NotaryID.isEmpty();
    const bool bIsSpecialMail = !bUsingNotary;
    // --------------------------------------------------
    MTCompose * compose_window = new MTCompose;
    compose_window->setAttribute(Qt::WA_DeleteOnClose);
    // --------------------------------------------------
    if (!myNymID.isEmpty()) // If there's a nym ID.
    {
        if (!myAddress.isEmpty())
            compose_window->setInitialSenderNym(myNymID, myAddress);
        else
            compose_window->setInitialSenderNym(myNymID);
    }
    else if (!myAddress.isEmpty())
        compose_window->setInitialSenderAddress(myAddress);
    // ---------------------------------------
    if (!otherNymID.isEmpty()) // If there's an "other nym ID".
    {
        if (!otherAddress.isEmpty())
            compose_window->setInitialRecipientNym(otherNymID, otherAddress);
        else
            compose_window->setInitialRecipientNym(otherNymID);
    }
    else if (!otherAddress.isEmpty())
        compose_window->setInitialRecipientAddress(otherAddress);
    // --------------------------------------------------
    if (bUsingNotary)
        compose_window->setInitialServer(NotaryID);
    // --------------------------------------------------
    compose_window->setInitialSubject(subject);
    // --------------------------------------------------
    if (nPaymentID > 0)
    {
        QString body        = MTContactHandler::getInstance()->GetPaymentBody(nPaymentID);
        QString pendingBody = MTContactHandler::getInstance()->GetPaymentPendingBody(nPaymentID);

        if (!body.isEmpty())
            compose_window->setInitialBody(body);
        else if (!pendingBody.isEmpty())
            compose_window->setInitialBody(pendingBody);
    }
    // --------------------------------------------------
    compose_window->setVariousIds(
                            bOutgoing ? myNymID : senderNymID,
                            bOutgoing ? recipientNymID : myNymID,
                            bOutgoing ? myAddress : senderAddr,
                            bOutgoing ? recipientAddr : myAddress);
    // --------------------------------------------------
    compose_window->dialog();
    Focuser f(compose_window);
    f.show();
    f.focus();
    // -----------------------------
    if (haveRepliedIndex.isValid())
        listRecordsToMarkAsReplied_.append(haveRepliedIndex);
    if (listRecordsToMarkAsReplied_.count() > 0)
        QTimer::singleShot(0, this, SLOT(on_MarkAsReplied_timer()));
}

void Activity::on_toolButtonForward_clicked()
{
    QPointer<ModelPayments> pModel = DBHandler::getInstance()->getPaymentModel();

    if (!pModel)
        return;
    // ------------------------------------
    QModelIndex proxyIndex = pCurrentTabTableView_->currentIndex();

    if (!proxyIndex.isValid())
        return;
    // ------------------------------------
    QModelIndex sourceIndex = pCurrentTabProxyModel_->mapToSource(proxyIndex);

    if (!sourceIndex.isValid())
        return;

    QModelIndex haveForwardedIndex = pModel->sibling(sourceIndex.row(),
                                                     PMNT_SOURCE_COL_HAVE_FORWARDED,
                                                     sourceIndex);
    // ------------------------------------
    QSqlRecord record = pModel->record(sourceIndex.row());

    if (record.isEmpty())
        return;
    // ------------------------------------
    const int nPaymentID = record.value(PMNT_SOURCE_COL_PMNT_ID).isValid() ? record.value(PMNT_SOURCE_COL_PMNT_ID).toInt() : 0;

    const bool bOutgoing = (0 == record.value(PMNT_SOURCE_COL_FOLDER).toInt());

    const QVariant qvar_method_type = record.value(PMNT_SOURCE_COL_METHOD_TYPE);
    const QString  methodType = qvar_method_type.isValid() ? qvar_method_type.toString() : "";

    const QVariant qvar_my_nym_id = record.value(PMNT_SOURCE_COL_MY_NYM);
    const QString  myNymID = qvar_my_nym_id.isValid() ? qvar_my_nym_id.toString() : "";

    const QVariant qvar_my_addr = record.value(PMNT_SOURCE_COL_MY_ADDR);
    const QString  myAddress = qvar_my_addr.isValid() ? qvar_my_addr.toString() : "";

    const QVariant qvar_sender_nym_id = record.value(PMNT_SOURCE_COL_SENDER_NYM);
    const QString  senderNymID = qvar_sender_nym_id.isValid() ? qvar_sender_nym_id.toString() : "";

    const QVariant qvar_recipient_nym_id = record.value(PMNT_SOURCE_COL_RECIP_NYM);
    const QString  recipientNymID = qvar_recipient_nym_id.isValid() ? qvar_recipient_nym_id.toString() : "";

    const QVariant qvar_notary_id = record.value(PMNT_SOURCE_COL_NOTARY_ID);
    const QString  NotaryID = qvar_notary_id.isValid() ? qvar_notary_id.toString() : "";

    const QVariant qvar_sender_addr = record.value(PMNT_SOURCE_COL_SENDER_ADDR);
    const QString  senderAddr = qvar_sender_addr.isValid() ? qvar_sender_addr.toString() : "";

    const QVariant qvar_recipient_addr = record.value(PMNT_SOURCE_COL_RECIP_ADDR);
    const QString  recipientAddr = qvar_recipient_addr.isValid() ? qvar_recipient_addr.toString() : "";

    const QVariant qvar_subject = record.value(PMNT_SOURCE_COL_MEMO);
    const QString  subject = qvar_subject.isValid() ? MTContactHandler::getInstance()->Decode(qvar_subject.toString()) : "";
    // --------------------------------------------------
    const QString& otherNymID = bOutgoing ? recipientNymID : senderNymID;
    const QString& otherAddress  = bOutgoing ? recipientAddr  : senderAddr;
    // --------------------------------------------------
    const bool bUsingNotary   = !NotaryID.isEmpty();
    const bool bIsSpecialMail = !bUsingNotary;
    // --------------------------------------------------
    MTCompose * compose_window = new MTCompose;
    compose_window->setAttribute(Qt::WA_DeleteOnClose);
    // --------------------------------------------------
    if (!myNymID.isEmpty()) // If there's a nym ID.
    {
        if (!myAddress.isEmpty())
            compose_window->setInitialSenderNym(myNymID, myAddress);
        else
            compose_window->setInitialSenderNym(myNymID);
    }
    else if (!myAddress.isEmpty())
        compose_window->setInitialSenderAddress(myAddress);
    // ---------------------------------------
//    if (!otherNymID.isEmpty()) // If there's an "other nym ID".
//    {
//        if (!otherAddress.isEmpty())
//            compose_window->setInitialRecipientNym(otherNymID, otherAddress);
//        else
//            compose_window->setInitialRecipientNym(otherNymID);
//    }
//    else if (!otherAddress.isEmpty())
//        compose_window->setInitialRecipientAddress(otherAddress);
    // --------------------------------------------------
    if (bUsingNotary)
        compose_window->setInitialServer(NotaryID);
    // --------------------------------------------------
    compose_window->setInitialSubject(subject);
    // --------------------------------------------------
    if (nPaymentID > 0)
    {
        QString body        = MTContactHandler::getInstance()->GetPaymentBody(nPaymentID);
        QString pendingBody = MTContactHandler::getInstance()->GetPaymentPendingBody(nPaymentID);

        if (!body.isEmpty())
            compose_window->setInitialBody(body);
        else if (!pendingBody.isEmpty())
            compose_window->setInitialBody(pendingBody);
    }
    // --------------------------------------------------
    compose_window->setForwarded();
    // --------------------------------------------------
    compose_window->setVariousIds(
                            bOutgoing ? myNymID : senderNymID,
                            bOutgoing ? recipientNymID : myNymID,
                            bOutgoing ? myAddress : senderAddr,
                            bOutgoing ? recipientAddr : myAddress);
    // --------------------------------------------------
    compose_window->dialog();
    Focuser f(compose_window);
    f.show();
    f.focus();
    // -----------------------------
    if (haveForwardedIndex.isValid())
        listRecordsToMarkAsForwarded_.append(haveForwardedIndex);
    if (listRecordsToMarkAsForwarded_.count() > 0)
        QTimer::singleShot(0, this, SLOT(on_MarkAsForwarded_timer()));
}



void Activity::on_toolButtonDelete_clicked()
{
    if ( (nullptr != pCurrentTabTableView_) &&
         (nullptr != pCurrentTabProxyModel_) )
    {
        if (!pCurrentTabTableView_->selectionModel()->hasSelection())
            return;
        // ----------------------------------------------
        QMessageBox::StandardButton reply;

        reply = QMessageBox::question(this, tr("Moneychanger"), QString("%1<br/><br/>%2").arg(tr("Are you sure you want to delete these receipts?")).
                                      arg(tr("WARNING: This is not reversible!")),
                                      QMessageBox::Yes|QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;
        // ----------------------------------------------
        QPointer<ModelPayments> pModel = DBHandler::getInstance()->getPaymentModel();

        if (pModel)
        {
            QItemSelection selection( pCurrentTabTableView_->selectionModel()->selection() );

            int nFirstProxyRowRemoved = -1;
            int nLastProxyRowRemoved  = -1;
            int nCountRowsRemoved     = 0;

            QList<int> rows, payment_ids;
            foreach( const QModelIndex & index, selection.indexes() ) {
                QModelIndex sourceIndex = pCurrentTabProxyModel_->mapToSource(index);
                rows.append( sourceIndex.row() );
                // --------------------------------
                nLastProxyRowRemoved = index.row();
                if ((-1) == nFirstProxyRowRemoved)
                    nFirstProxyRowRemoved = index.row();
            }

            qSort( rows );

            bool bRemoved = false;

            int prev = -1;

            for(int ii = rows.count() - 1; ii >= 0; ii -= 1 ) {
               int current = rows[ii];
               if( current != prev ) {
                   bRemoved = true;
                   QModelIndex sourceIndexpmntid = pModel->index(current, PMNT_SOURCE_COL_PMNT_ID);
                   if (sourceIndexpmntid.isValid())
                       payment_ids.append(pModel->data(sourceIndexpmntid).toInt());
                   pModel->removeRows( current, 1 );
                   prev = current;
                   nCountRowsRemoved++;
               }
            }

            if (bRemoved)
            {
                if (pModel->submitAll())
                {
                    pModel->database().commit();
                    // ------------------------
                    // Now we just deleted some receipts; let's delete also the corresponding
                    // receipt contents. (We saved the deleted IDs for this purpose.)
                    //
                    for (int ii = 0; ii < payment_ids.count(); ++ii)
                    {
                        const int nPmntID = payment_ids[ii];

                        if (nPmntID > 0)
                            if (!MTContactHandler::getInstance()->DeletePaymentBody(nPmntID))
                                qDebug() << "Payments::on_toolButtonDelete_clicked: Failed trying to delete payment body with payment_id: " << nPmntID << "\n";
                    }
                    // ------------------------
                    // We just deleted the selected rows.
                    // So now we need to choose another row to select.

                    int nRowToSelect = -1;

                    if ((nFirstProxyRowRemoved >= 0) && (nFirstProxyRowRemoved < pCurrentTabProxyModel_->rowCount()))
                        nRowToSelect = nFirstProxyRowRemoved;
                    else if (0 == nFirstProxyRowRemoved)
                        nRowToSelect = 0;
                    else if (nFirstProxyRowRemoved > 0)
                        nRowToSelect = pCurrentTabProxyModel_->rowCount() - 1;
                    else
                        nRowToSelect = 0;

                    if ((pCurrentTabProxyModel_->rowCount() > 0) && (nRowToSelect >= 0) &&
                            (nRowToSelect < pCurrentTabProxyModel_->rowCount()))
                    {
                        QModelIndex previous = pCurrentTabTableView_->currentIndex();
                        pCurrentTabTableView_->blockSignals(true);
                        pCurrentTabTableView_->selectRow(nRowToSelect);
                        pCurrentTabTableView_->blockSignals(false);

                        if (pCurrentTabTableView_ == ui->tableViewReceived)
                            on_tableViewReceivedSelectionModel_currentRowChanged(pCurrentTabTableView_->currentIndex(), previous);
                        else
                            on_tableViewSentSelectionModel_currentRowChanged(pCurrentTabTableView_->currentIndex(), previous);
                    }
                }
                else
                {
                    pModel->database().rollback();
                    qDebug() << "Database Write Error" <<
                               "The database reported an error: " <<
                               pModel->lastError().text();
                }
            }
        }
    }
}

void Activity::on_toolButtonRefresh_clicked()
{
    emit needToDownloadAccountData();
}

void Activity::onRecordlistPopulated()
{
    RefreshAll();
}

void Activity::onBalancesChanged()
{
    RefreshUserBar();
}

// The balances hasn't necessarily changed.
// (Perhaps the default account was changed.)
void Activity::onNeedToRefreshUserBar()
{
    RefreshUserBar();
}

void Activity::onNeedToRefreshRecords()
{
    RefreshAll();
}


void Activity::on_pushButtonSearchConversations_clicked()
{
    QString qstrSearchText = ui->lineEditSearchConversations->text();

    this->doSearchConversations(qstrSearchText.simplified());
}


void Activity::on_pushButtonSearchPayments_clicked()
{
    QString qstrSearchText = ui->lineEditSearchPayments->text();

    this->doSearchPayments(qstrSearchText.simplified());
}

void Activity::doSearchConversations(QString qstrInput)
{
    if (pThreadItemsProxyModel_)
    {
        pThreadItemsProxyModel_->setFilterString(qstrInput);
    }

//  RefreshConversationsTab(); // NOTE: Possibly not necessary since setting the filter string might be all it takes in this case.
}

void Activity::doSearchPayments(QString qstrInput)
{
    if (pPmntProxyModelInbox_)
    {
        pPmntProxyModelInbox_ ->setFilterString(qstrInput);
    }
    if (pPmntProxyModelOutbox_)
    {
        pPmntProxyModelOutbox_->setFilterString(qstrInput);
    }

    RefreshPayments();
}

void Activity::on_lineEditSearchPayments_textChanged(const QString &arg1)
{
    // This means someone clicked the "clear" button on the search box.
    if (arg1.isEmpty())
        doSearchPayments(arg1);
}

void Activity::on_lineEditSearchPayments_returnPressed()
{
    QString qstrSearchText = ui->lineEditSearchPayments->text();

    this->doSearchPayments(qstrSearchText.simplified());
}


void Activity::on_lineEditSearchConversations_textChanged(const QString &arg1)
{
    // This means someone clicked the "clear" button on the search box.
    if (arg1.isEmpty())
        doSearchConversations(arg1);
}

void Activity::on_lineEditSearchConversations_returnPressed()
{
    QString qstrSearchText = ui->lineEditSearchConversations->text();

    this->doSearchConversations(qstrSearchText.simplified());
}

void Activity::on_treeWidgetAccounts_customContextMenuRequested(const QPoint &pos)
{
    treeWidgetAccounts_PopupMenu(pos, ui->treeWidgetAccounts);
}

void Activity::resetPopupMenus()
{
    if (popupMenuPayments_) {
        popupMenuPayments_->close();
    }
    if (popupMenuAccounts_) {
        popupMenuAccounts_->close();
    }

    popupMenuPayments_.reset();
    popupMenuAccounts_.reset();

    pActionViewContact         = nullptr;
    pActionCreateContact       = nullptr;
    pActionExistingContact     = nullptr;
    pActionAcceptIncoming      = nullptr;
    pActionCancelOutgoing      = nullptr;
    pActionDiscardOutgoingCash = nullptr;
    pActionDiscardIncoming     = nullptr;
    pActionDownloadCredentials = nullptr;
    pActionOpenNewWindow       = nullptr;
    pActionReply               = nullptr;
    pActionForward             = nullptr;
    pActionDelete              = nullptr;
    pActionMarkRead            = nullptr;
    pActionMarkUnread          = nullptr;
    pActionPairWithSNP         = nullptr;
    pActionManageNotaries      = nullptr;
    pActionManageAssets        = nullptr;
    pActionManageContacts      = nullptr;
    pActionManageAccounts      = nullptr;

    pActionContactMsg          = nullptr;
    pActionContactPay          = nullptr;
    pActionContactInvoice      = nullptr;
    pActionContactRecurring    = nullptr;
}

/*
#define ACTIVITY_TREE_HEADER_TOTALS   0
#define ACTIVITY_TREE_HEADER_LOCAL    1
#define ACTIVITY_TREE_HEADER_SNP      2
#define ACTIVITY_TREE_HEADER_HOSTED   3
#define ACTIVITY_TREE_HEADER_CONTACTS 4

#define AC_NODE_TYPE_HEADER          0
#define AC_NODE_TYPE_ASSET_TOTAL     1
#define AC_NODE_TYPE_CONTACT         2
#define AC_NODE_TYPE_LOCAL_WALLET    3
#define AC_NODE_TYPE_LOCAL_ACCOUNT   4
#define AC_NODE_TYPE_SNP_NOTARY      5
#define AC_NODE_TYPE_SNP_ACCOUNT     6
#define AC_NODE_TYPE_HOSTED_NOTARY   7
#define AC_NODE_TYPE_HOSTED_ACCOUNT  8
 *
 * So UserData(0) should be the NODE TYPE. For the first row, it will be a header (Totals)
 * and so the "node type" should be: AC_NODE_TYPE_HEADER and "header" will be ACTIVITY_TREE_HEADER_TOTALS.
 *
 * For the second row, it will be an asset total (probably BTC or USD) so the "node type"
 * would be: AC_NODE_TYPE_ASSET_TOTAL. Again, set in UserData(0).
 *
 * EVERY row in the tree is also under a "header", and so should have the header set.
 * Continuing the above example, the first row has "header" of ACTIVITY_TREE_HEADER_TOTALS
 * and the second row also has "header" of ACTIVITY_TREE_HEADER_TOTALS. This should probably
 * be UserData(1).
 *
 * IF the node type is AC_NODE_TYPE_ASSET_TOTAL, as seen just above for the second row,
 * we probably also want to have the TLA available. This should probably be UserData(2).
 *
 * IF the node type is instead, say, AC_NODE_TYPE_SNP_ACCOUNT or AC_NODE_TYPE_HOSTED_ACCOUNT,
 * tracking account instead of asset ID, then in that case UserData(2) should contain the TLA
 * as before (for consistency's sake). Therefore the account ID should be in UserData(4).
 *
 * That way I can put the Opentxs asset type ID (if one exists) into UserData(3).
 *
 * Let's say instead the "node type" is AC_NODE_TYPE_HOSTED_NOTARY -- in that case, the "header"
 * is ACTIVITY_TREE_HEADER_HOSTED. So that's user data 0 and 1.
 * Then UserData(2) would contain the Opentxs Notary ID, if one was available. (Which is possible,
 * at least in the case of AC_NODE_TYPE_SNP_NOTARY).
 * NO -- this should also contain the TLA in 2, the asset type in 3, and the account ID in 4,
 * IF APPLICABLE, since the notary MAY display a "rolled up" balance total as well, and I prefer
 * to have those values available if they are relevant, in a way consistent with the above.
 * (However, once again, I cannot assume they are present. They may not be.)
 *
 * Therefore the Notary ID must be in UserData(5)! IF it's present at all.
 *
 * Therefore if it turns out to be a Contact, "node type" AC_NODE_TYPE_CONTACT, the Contact ID
 * itself, a new-style string-based Opentxs Contact ID, will preferably be in UserData(6).
 */
void Activity::treeWidgetAccounts_PopupMenu(const QPoint &pos, QTreeWidget * pTreeWidget)
{
    const std::string str_my_nym_id = Moneychanger::It()->get_default_nym_id().toStdString();

    int nTreeNodeType{-1};
    int nUnderHeader {-1};

    QString  qstrTLA;
    QString  qstrAssetTypeId;
    QString  qstrAccountId;
    QString  qstrServerId;
    QString  qstrContactId;
    // ------------------------
    QTreeWidgetItem * pTreeItem = pTreeWidget->itemAt(pos); // Might be Null...
    // ------------------------
    if ( nullptr != pTreeItem )
    {
        const QVariant qvarTreeNodeType = pTreeItem->data(0, Qt::UserRole  );
        nTreeNodeType = qvarTreeNodeType.isValid() ? qvarTreeNodeType.toInt() : -1;
        // ------------------------
        const QVariant qvarUnderHeader = pTreeItem->data(0, Qt::UserRole+1);
        nUnderHeader = qvarUnderHeader.isValid() ? qvarUnderHeader.toInt() : -1;
        // ------------------------
        if(   (-1) != nTreeNodeType
           && (-1) != nUnderHeader)
        {
            const QVariant qvarTLA = pTreeItem->data(0, Qt::UserRole+2);
            const QVariant qvarAssetTypeId = pTreeItem->data(0, Qt::UserRole+3);
            const QVariant qvarAccountId = pTreeItem->data(0, Qt::UserRole+4);
            const QVariant qvarServerId = pTreeItem->data(0, Qt::UserRole+5);
            const QVariant qvarContactId = pTreeItem->data(0, Qt::UserRole+6);
            // ------------------------
            qstrTLA = qvarTLA.isValid() ? qvarTLA.toString() : QString("");
            qstrAssetTypeId = qvarAssetTypeId.isValid() ? qvarAssetTypeId.toString() : QString("");
            qstrAccountId = qvarAccountId.isValid() ? qvarAccountId.toString() : QString("");
            qstrServerId = qvarServerId.isValid() ? qvarServerId.toString() : QString("");
            qstrContactId = qvarContactId.isValid() ? qvarContactId.toString() : QString("");
        }
    }
    // ------------------------
    /*
    #define ACTIVITY_TREE_HEADER_TOTALS   0
    #define ACTIVITY_TREE_HEADER_LOCAL    1
    #define ACTIVITY_TREE_HEADER_SNP      2
    #define ACTIVITY_TREE_HEADER_HOSTED   3
    #define ACTIVITY_TREE_HEADER_CONTACTS 4

    #define AC_NODE_TYPE_HEADER          0
    #define AC_NODE_TYPE_ASSET_TOTAL     1
    #define AC_NODE_TYPE_CONTACT         2
    #define AC_NODE_TYPE_LOCAL_WALLET    3
    #define AC_NODE_TYPE_LOCAL_ACCOUNT   4
    #define AC_NODE_TYPE_SNP_NOTARY      5
    #define AC_NODE_TYPE_SNP_ACCOUNT     6
    #define AC_NODE_TYPE_HOSTED_NOTARY   7
    #define AC_NODE_TYPE_HOSTED_ACCOUNT  8
    */

    // By this point we know for a fact that no matter what the user clicked on,
    // the necessary data is available from that item to construct the appropriate menu.
    //
    resetPopupMenus();
    popupMenuAccounts_.reset(new QMenu(this));
    // --------------------------------------------------
    const bool bSelectedNotary = !qstrServerId.isEmpty() &&
                                 (   AC_NODE_TYPE_SNP_NOTARY    == nTreeNodeType
                                  || AC_NODE_TYPE_HOSTED_NOTARY == nTreeNodeType );

    const bool bSelectedAcctLowLevel = !qstrAccountId.isEmpty() &&
                                        (   AC_NODE_TYPE_SNP_ACCOUNT    == nTreeNodeType
                                         || AC_NODE_TYPE_HOSTED_ACCOUNT == nTreeNodeType);

    // If the user has selected an actual account tree node, or he has selected
    // a notary tree node that also happens to be rolled up from a single account ID,
    // either way, there's an account ID and the user has "selected an account".
    //
    const bool bSelectedAccount =  (bSelectedAcctLowLevel ||
                                    (bSelectedNotary && !qstrAccountId.isEmpty()) );
    const bool bSelectedAsset   = !qstrAssetTypeId.isEmpty();
    const bool bSelectedContact = (ACTIVITY_TREE_HEADER_CONTACTS == nUnderHeader) && !qstrContactId.isEmpty();
    // --------------------------------------------------
    if (AC_NODE_TYPE_ASSET_TOTAL == nTreeNodeType) {
        pActionManageAssets = popupMenuAccounts_->addAction(tr("Manage asset types"));
        popupMenuAccounts_->addSeparator();
    }
    // The user has right-clicked on a CONTACT.
    // (That is, he's right-clicked on a valid non-header item that appears under the "Contacts" header).
    //
    else if (bSelectedContact) {
        pActionViewContact = popupMenuAccounts_->addAction(tr("View this contact"));
        if (opentxs::Messagability::READY == opentxs::OT::App().API().OTME_TOO().CanMessage(str_my_nym_id, qstrContactId.toStdString()))
        {
            pActionContactMsg = popupMenuAccounts_->addAction(tr("Message this contact"));
            pActionContactPay = popupMenuAccounts_->addAction(tr("Send payment"));
            pActionContactInvoice = popupMenuAccounts_->addAction(tr("Request payment"));
            pActionContactRecurring = popupMenuAccounts_->addAction(tr("Request recurring payments"));
        }
        //pActionExistingContact = popupMenuAccounts_->addAction(tr("Merge with existing Opentxs Contact"));
        //pActionDownloadCredentials = popupMenuAccounts_->addAction(tr("Re-download contact"));
        popupMenuAccounts_->addSeparator();
    }
    else if (bSelectedAccount) {
        pActionManageAccounts = popupMenuAccounts_->addAction(tr("View account"));
    }
    // -----------------
    if (bSelectedNotary) {
        pActionManageNotaries = popupMenuAccounts_->addAction(tr("View notary"));
    }
    if (bSelectedAsset && (nullptr == pActionManageAssets)) {
        pActionManageAssets = popupMenuAccounts_->addAction(tr("View asset type"));
    }
    popupMenuAccounts_->addSeparator();
    // --------------------------------------------------
    if (nullptr == pActionManageContacts && nullptr == pActionViewContact) {
        pActionManageContacts = popupMenuAccounts_->addAction(tr("Manage contacts"));
    }
    if (!bSelectedAccount && (nullptr == pActionManageAccounts)) {
        pActionManageAccounts = popupMenuAccounts_->addAction(tr("Manage accounts"));
    }
    if (!bSelectedNotary && (nullptr == pActionManageNotaries)) {
        pActionManageNotaries = popupMenuAccounts_->addAction(tr("Manage notaries"));
    }
    if (!bSelectedAsset && (nullptr == pActionManageAssets)) {
        pActionManageAssets = popupMenuAccounts_->addAction(tr("Manage asset types"));
    }
    popupMenuAccounts_->addSeparator();
    // --------------------------------------------------
    if (!bSelectedContact)
        pActionCreateContact = popupMenuAccounts_->addAction(tr("Create new contact"));
    // --------------------------------------------------
    popupMenuAccounts_->addSeparator();
    // --------------------------------------------------
    pActionPairWithSNP = popupMenuAccounts_->addAction(tr("Pair a Stash Node Pro"));
    // --------------------------------------------------
    QPoint globalPos = pTreeWidget->mapToGlobal(pos);
    const QAction* selectedAction = popupMenuAccounts_->exec(globalPos); // Here we popup the menu, and get the user's click.
    if (nullptr == selectedAction)
        return;
    // ----------------------------------
    if (selectedAction == pActionViewContact)
    {
        if (nullptr != pTreeItem)
            pTreeItem->setSelected(true);

        if (!qstrContactId.isEmpty())
        {
            emit showContact(qstrContactId);
        }
        return;
    }
    // ----------------------------------
    // Merge 2 existing opentxs contacts together.
    //
    else if (selectedAction == pActionExistingContact)
    {
        if (nullptr != pTreeItem)
            pTreeItem->setSelected(true);

        // This should never happen since we wouldn't even have gotten this menu option
        // in the first place, unless contact ID had been 0.
        if (qstrContactId.isEmpty())
            return;

        // TODO:  Make sure there are 2 or more Contacts selected first?

        // Then call Justus new Contact merging function.

        // until then...

        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionDownloadCredentials)
    {
        if (nullptr != pTreeItem)
            pTreeItem->setSelected(true);

        if (qstrContactId.isEmpty())
            return;

        if (str_my_nym_id.empty()) {
            qDebug() << "Unable to download credentials without a default Nym ID being set";
            return;
        }
        opentxs::OT::App().API().OTME_TOO().CanMessage(str_my_nym_id, qstrContactId.toStdString());

        // Todo: emit some signal in a few seconds that an Opentxs Contact was just re-downloaded recently probably?

    }
    // ----------------------------------------------
    else if (selectedAction == pActionCreateContact)
    {
        if (nullptr != pTreeItem)
            pTreeItem->setSelected(true);

        MTGetStringDialog nameDlg(this, tr("Enter a display label for the new contact"));

        if (QDialog::Accepted != nameDlg.exec())
            return;
        // --------------------------------------
        const QString strNewContactLabel = nameDlg.GetOutputString();
        const std::string str_new_contact_label = strNewContactLabel.toStdString();
        // --------------------------------------------------
        auto pContact = opentxs::OT::App().Contact().NewContact(str_new_contact_label);

        if (!pContact) {
            qDebug() << "Error: Failed trying to create new Contact.";
            return;
        }
        // -----------------------------------------------------
        const opentxs::Identifier idContact{pContact->ID()};
        const opentxs::String     strContact(idContact);
        const std::string         str_contact(strContact.Get());
        qstrContactId = QString::fromStdString(str_contact);
        // -----------------------------------------------------
        if (!qstrContactId.isEmpty())
        {
            emit showContactAndRefreshHome(qstrContactId);
        }
        return;
    }
    // ----------------------------------------------
    else if (selectedAction == pActionPairWithSNP)
    {
        emit needToPairStashNode();
        return;
    }
    // ----------------------------------------------
    else if (selectedAction == pActionManageNotaries)
    {
        emit showServer(qstrServerId); // qstrServerId may be empty; that's okay.
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionManageAssets)
    {
        emit showAsset(qstrAssetTypeId); // asset ID may be empty, which is okay.
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionManageAccounts)
    {
        emit showAccount(qstrAccountId); // account ID may be empty, which is okay.
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionManageContacts)
    {
        emit showContacts();
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionContactMsg)
    {
        emit messageContact(QString::fromStdString(str_my_nym_id), qstrContactId);
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionContactPay)
    {
        // todo: qstrAccountId is empty here, so maybe should just have
        // "pay to contact" signal.
        emit payFromAccountToContact(qstrAccountId, qstrContactId);
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionContactInvoice)
    {
        // todo: qstrAccountId is empty here, so maybe should just have
        // "request from contact" signal.
        emit requestToAccountFromContact(qstrAccountId, qstrContactId);
        return;
    }
    // ----------------------------------
    else if (selectedAction == pActionContactRecurring)
    {
        // todo: qstrAccountId is empty here, so maybe should just have
        // "propose from contact" signal.
        emit proposeToAccountFromContact(qstrAccountId, qstrContactId);
        return;
    }
    // ----------------------------------
    popupMenuAccounts_->close();
}


void Activity::on_tabWidgetMain_currentChanged(int index)
{
    /*
     * 0 Accounts
     * 1 Conversations
     * 2 Messages
     * 3 Agreements
     * 4 Exchange
     * 5 Tools
     * 6 Settings
     */
    switch (index) {
    case 0: {
        nSelectedTab_ = index;
    } break;
    case 1: {
        nSelectedTab_ = index;
    } break;
    case 2: { // Messages
        ui->tabWidgetMain->blockSignals(true);
        ui->tabWidgetMain->setCurrentIndex(nSelectedTab_); // Set it back (don't allow the user to change to this tab)
        ui->tabWidgetMain->blockSignals(false);

        // Then pop up the messages dialog:
        emit showMessages();
        return;
    } break;
    case 3: { // Active Agreements
        ui->tabWidgetMain->blockSignals(true);
        ui->tabWidgetMain->setCurrentIndex(nSelectedTab_); // Set it back (don't allow the user to change to this tab)
        ui->tabWidgetMain->blockSignals(false);

        // Then pop up the agreements dialog:
        emit showActiveAgreements();
        return;
    } break;
    case 4: { // Exchange
        ui->tabWidgetMain->blockSignals(true);
        ui->tabWidgetMain->setCurrentIndex(nSelectedTab_); // Set it back (don't allow the user to change to this tab)
        ui->tabWidgetMain->blockSignals(false);

        // Then pop up the exchange dialog:
        emit showExchange();
        return;
    } break;
    case 5: { // Tools
        nSelectedTab_ = index;
    } break;
    case 6: { // Settings
        ui->tabWidgetMain->blockSignals(true);
        ui->tabWidgetMain->setCurrentIndex(nSelectedTab_); // Set it back (don't allow the user to change to this tab)
        ui->tabWidgetMain->blockSignals(false);

        // Then pop up the settings dialog:
        emit showSettings();
        return;
    } break;
    default: { // should never happen
        qDebug() << QString("Activity::on_tabWidgetMain_currentChanged: Bad index: %1").arg(index);
        return;
    } break;
    }
}

Activity::Activity(QWidget *parent) :
    QWidget(parent, Qt::Window),
    ui(new Ui::Activity)
{
    ui->setupUi(this);

    this->installEventFilter(this);

    connect(this, SIGNAL(showMessages()), Moneychanger::It(), SLOT(mc_messages_slot()));
    connect(this, SIGNAL(showActiveAgreements()), Moneychanger::It(), SLOT(mc_agreements_slot()));
    connect(this, SIGNAL(showExchange()), Moneychanger::It(), SLOT(mc_market_slot()));
    connect(this, SIGNAL(showSettings()), Moneychanger::It(), SLOT(mc_settings_slot()));
    connect(this, SIGNAL(needToCheckNym(QString, QString, QString)), Moneychanger::It(), SLOT(onNeedToCheckNym(QString, QString, QString)));
    connect(this, SIGNAL(showServer(QString)), Moneychanger::It(), SLOT(mc_show_server_slot(QString)));
    connect(this, SIGNAL(showAsset(QString)),  Moneychanger::It(), SLOT(mc_show_asset_slot(QString)));
    connect(this, SIGNAL(needToPairStashNode()), Moneychanger::It(), SLOT(mc_pair_node_slot()));
    connect(this, SIGNAL(showAccount(QString)),  Moneychanger::It(), SLOT(mc_show_account_slot(QString)));
    connect(this, SIGNAL(messageContact(QString,QString)), Moneychanger::It(), SLOT(mc_message_contact_slot(QString,QString)));
    connect(this, SIGNAL(payFromAccountToContact(QString,QString)), Moneychanger::It(), SLOT(mc_send_from_acct_to_contact(QString,QString)));
    connect(this, SIGNAL(requestToAccountFromContact(QString,QString)), Moneychanger::It(), SLOT(mc_request_to_acct_from_contact(QString,QString)));
    connect(this, SIGNAL(proposeToAccountFromContact(QString,QString)), Moneychanger::It(), SLOT(mc_proposeplan_to_acct_from_contact(QString,QString)));
    // --------------------------------------------------
    connect(this, SIGNAL(sig_on_toolButton_payments_clicked()), Moneychanger::It(), SLOT(mc_payments_slot()));
    connect(this, SIGNAL(sig_on_toolButton_pending_clicked()), Moneychanger::It(), SLOT(mc_overview_slot()));
    connect(this, SIGNAL(sig_on_toolButton_markets_clicked()), Moneychanger::It(), SLOT(mc_market_slot()));
    connect(this, SIGNAL(sig_on_toolButton_secrets_clicked()), Moneychanger::It(), SLOT(mc_passphrase_manager_slot()));
    connect(this, SIGNAL(sig_on_toolButton_importCash_clicked()), Moneychanger::It(), SLOT(mc_import_slot()));
    connect(this, SIGNAL(sig_on_toolButton_contacts_clicked()), Moneychanger::It(), SLOT(mc_addressbook_slot()));
    connect(this, SIGNAL(sig_on_toolButton_smartContracts_clicked()), Moneychanger::It(), SLOT(mc_smartcontract_slot()));
    connect(this, SIGNAL(sig_on_toolButton_Corporations_clicked()), Moneychanger::It(), SLOT(mc_corporation_slot()));
    connect(this, SIGNAL(sig_on_toolButton_transport_clicked()), Moneychanger::It(), SLOT(mc_transport_slot()));
    connect(this, SIGNAL(sig_on_toolButton_quit_clicked()), Moneychanger::It(), SLOT(mc_shutdown_slot()));
    connect(this, SIGNAL(sig_on_toolButton_encrypt_clicked()), Moneychanger::It(), SLOT(mc_crypto_encrypt_slot()));
    connect(this, SIGNAL(sig_on_toolButton_sign_clicked()), Moneychanger::It(), SLOT(mc_crypto_sign_slot()));
    connect(this, SIGNAL(sig_on_toolButton_decrypt_clicked()), Moneychanger::It(), SLOT(mc_crypto_decrypt_slot()));
    connect(this, SIGNAL(sig_on_toolButton_liveAgreements_clicked()), Moneychanger::It(), SLOT(mc_agreements_slot()));
}

void Activity::on_toolButton_payments_clicked() { emit sig_on_toolButton_payments_clicked(); }
void Activity::on_toolButton_pending_clicked() { emit sig_on_toolButton_pending_clicked(); }
void Activity::on_toolButton_markets_clicked() { emit sig_on_toolButton_markets_clicked(); }
void Activity::on_toolButton_importCash_clicked() { emit sig_on_toolButton_importCash_clicked(); }
void Activity::on_toolButton_contacts_clicked() { emit sig_on_toolButton_contacts_clicked(); }
void Activity::on_toolButton_smartContracts_clicked() { emit sig_on_toolButton_smartContracts_clicked(); }
void Activity::on_toolButton_Corporations_clicked() { emit sig_on_toolButton_Corporations_clicked(); }
void Activity::on_toolButton_quit_clicked() { emit sig_on_toolButton_quit_clicked(); }
void Activity::on_toolButton_secrets_clicked() { emit sig_on_toolButton_secrets_clicked(); }
void Activity::on_toolButton_encrypt_clicked() { emit sig_on_toolButton_encrypt_clicked(); }
void Activity::on_toolButton_sign_clicked() { emit sig_on_toolButton_sign_clicked(); }
void Activity::on_toolButton_decrypt_clicked() { emit sig_on_toolButton_decrypt_clicked(); }
void Activity::on_toolButton_transport_clicked() { emit sig_on_toolButton_transport_clicked(); }
void Activity::on_toolButton_liveAgreements_clicked() { emit sig_on_toolButton_liveAgreements_clicked(); }


#ifndef CUSTOMTABLE_H
#define CUSTOMTABLE_H

#include <QTableWidget>

namespace Frontend
{

class CustomTable : public QTableWidget
{
    Q_OBJECT

public:
    CustomTable(QWidget* pParent = nullptr);
    virtual ~CustomTable();

    QStringList horizontalHeaderLabels();
    void setDataAlignment(Qt::Alignment alignment);

signals:
    void pasted();

private:
    void createActions();
    void copySelection();
    void pasteSelection();
    void clearSelection();
    void processContextMenuRequested(QPoint const& position);
};

}

#endif // CUSTOMTABLE_H

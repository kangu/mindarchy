#pragma once
#include <QString>
#include <QVector>
#include <QSet>
#include <algorithm>
#include <limits>

namespace Search {
struct Normalized { QString text; QVector<int> positions; };
inline Normalized normalize(const QString &source) {
    Normalized result;
    for(int i=0;i<source.size();++i) {
        for(QChar c:QString(source[i]).normalized(QString::NormalizationForm_D).toCaseFolded()) {
            if(c.category()==QChar::Mark_NonSpacing) continue;
            result.text+=c.isLetterOrNumber() ? c : QChar(' '); result.positions.append(i);
        }
    }
    return result;
}
struct Match { bool found=false; int score=0; QSet<int> positions; };
inline Match match(const QString &source,const QString &query) {
    const auto normalized=normalize(source);
    const QString text=normalized.text;
    const auto terms=normalize(query.left(256)).text.split(' ',Qt::SkipEmptyParts);
    Match result; if(terms.isEmpty()) return result;
    for(const auto &term:terms) {
        QVector<int> bestPositions; int best=std::numeric_limits<int>::max();
        int exact=text.indexOf(term);
        if(exact>=0) {
            best=0;
            while(exact>=0) {
                for(int i=0;i<term.size();++i) bestPositions.append(exact+i);
                exact=text.indexOf(term,exact+term.size());
            }
        } else for(int start=0;start<text.size();) {
            if(text[start]==' ') { ++start; continue; }
            int end=text.indexOf(' ',start); if(end<0) end=text.size();
            const auto word=text.mid(start,end-start);
            QVector<int> sequence; int at=0;
            for(int j=0;j<word.size();++j) if(at<term.size() && word[j]==term[at]) { sequence.append(start+j); ++at; }
            const int score=10+word.size()-term.size();
            if(term.size()>=2 && at==term.size() && score<best) { best=score; bestPositions=sequence; }
            const int tolerance=term.size()>=6 ? 2 : term.size()>=3 ? 1 : 0;
            if(tolerance && std::abs(word.size()-term.size())<=tolerance && word.size()<=258) {
                QVector<QVector<int>> distance(term.size()+1,QVector<int>(word.size()+1));
                for(int i=0;i<=term.size();++i) distance[i][0]=i;
                for(int j=0;j<=word.size();++j) distance[0][j]=j;
                for(int i=1;i<=term.size();++i) for(int j=1;j<=word.size();++j)
                    distance[i][j]=std::min({distance[i-1][j]+1,distance[i][j-1]+1,distance[i-1][j-1]+(term[i-1]!=word[j-1])});
                const int errors=distance.last().last();
                if(errors<=tolerance && 20+errors<best) {
                    best=20+errors; bestPositions.clear();
                    int i=term.size(),j=word.size();
                    while(i>0 && j>0) {
                        if(distance[i][j]==distance[i-1][j-1]+(term[i-1]!=word[j-1])) {
                            if(term[i-1]==word[j-1]) bestPositions.append(start+j-1);
                            --i; --j;
                        } else if(distance[i][j]==distance[i-1][j]+1) --i;
                        else --j;
                    }
                }
            }
            start=end;
        }
        if(best==std::numeric_limits<int>::max()) return {};
        result.score+=best;
        for(int index:bestPositions) result.positions.insert(normalized.positions[index]);
    }
    result.found=true; return result;
}
}

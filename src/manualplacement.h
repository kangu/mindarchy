#pragma once
#include <QRectF>
#include <QVector>
#include <QSet>
#include <algorithm>

namespace ManualPlacement {
// Work in primary/cross axes so horizontal and vertical maps share the policy.
template<class Nodes>
QPointF nextChildCenter(const Nodes &nodes, const QVector<int> &visible, int parent,
                       QSizeF size, bool vertical, int after) {
    const QSet<int> visibleSet(visible.begin(),visible.end());
    auto primary=[vertical](QPointF p) { return vertical?p.y():p.x(); };
    auto cross=[vertical](QPointF p) { return vertical?p.x():p.y(); };
    auto point=[vertical](qreal a,qreal b) { return vertical?QPointF(b,a):QPointF(a,b); };
    auto median=[](QVector<qreal> values,qreal fallback) {
        if(values.isEmpty()) return fallback;
        std::sort(values.begin(),values.end()); return values[values.size()/2];
    };
    const auto parentRect=nodes.value(parent).rect;
    const auto parentCenter=parentRect.center();
    const auto children=nodes.value(parent).children;
    qreal direction=!vertical && parent!=1 && parentCenter.x()<nodes.value(1).rect.center().x()?-1:1;
    if(!children.isEmpty()) {
        const int reference=children.contains(after)?after:children.last();
        if(nodes.contains(reference)) direction=primary(nodes.value(reference).rect.center())<primary(parentCenter)?-1:1;
    }
    QVector<int> siblings;
    QVector<qreal> alignments, steps;
    for(int child:children) if(visibleSet.contains(child)) {
        const auto rect=nodes.value(child).rect;
        if((primary(rect.center())-primary(parentCenter))*direction<0) continue;
        siblings.append(child);
        alignments.append(primary(rect.center())-direction*(vertical?rect.height():rect.width())/2);
        if(siblings.size()>1) steps.append(cross(rect.center())-cross(nodes.value(siblings[siblings.size()-2]).rect.center()));
    }
    const qreal flow=median(steps,1)<0?-1:1;
    auto subtreeBounds=[&](int root) {
        QRectF bounds; QVector<int> pending{root};
        while(!pending.isEmpty()) {
            const int id=pending.takeLast(); if(!nodes.contains(id)) continue;
            const auto node=nodes.value(id);
            if(visibleSet.contains(id)) bounds=bounds.united(node.rect);
            if(!node.folded) pending+=node.children;
        }
        return bounds;
    };
    QVector<qreal> gaps;
    for(int i=1;i<siblings.size();++i) {
        const auto a=subtreeBounds(siblings[i-1]),b=subtreeBounds(siblings[i]);
        const qreal gap=(cross(b.center())-cross(a.center()))*flow
            -(vertical?a.width()+b.width():a.height()+b.height())/2;
        if(gap>0) gaps.append(gap);
    }
    const qreal gap=std::clamp(median(gaps,32),24.,64.);
    const qreal primarySize=vertical?size.height():size.width();
    const qreal crossSize=vertical?size.width():size.height();
    const qreal edge=primary(parentCenter)+direction*((vertical?parentRect.height():parentRect.width())/2+64);
    const qreal along=median(alignments,edge)+direction*primarySize/2;
    qreal across=cross(parentCenter);
    // Append after the complete occupied branch band, never among its descendants.
    if(!siblings.isEmpty()) {
        qreal boundary=-1e100;
        for(int sibling:siblings) {
            const auto bounds=subtreeBounds(sibling);
            boundary=std::max(boundary,flow*cross(bounds.center())+(vertical?bounds.width():bounds.height())/2);
        }
        across=flow*(boundary+gap+crossSize/2);
        // A Return-created sibling may use a genuine gap after the selected subtree.
        const int index=siblings.indexOf(after);
        if(index>=0 && index+1<siblings.size()) {
            const auto a=subtreeBounds(after),b=subtreeBounds(siblings[index+1]);
            const qreal end=flow*cross(a.center())+(vertical?a.width():a.height())/2;
            const qreal next=flow*cross(b.center())-(vertical?b.width():b.height())/2;
            if(next-end>=crossSize+2*gap) across=flow*(end+gap+crossSize/2);
        }
    }
    // Move monotonically past colliding nodes, including unrelated branches.
    for(int pass=0;pass<=visible.size();++pass) {
        QRectF candidate(QPointF(),size); candidate.moveCenter(point(along,across));
        qreal next=across*flow;
        for(int id:visible) {
            const auto obstacle=nodes.value(id).rect;
            if(candidate.intersects(obstacle.adjusted(-gap/2,-gap/2,gap/2,gap/2)))
                next=std::max(next,flow*cross(obstacle.center())+(vertical?obstacle.width():obstacle.height())/2+gap+crossSize/2);
        }
        if(next==across*flow) break;
        across=next*flow;
    }
    return point(along,across);
}
}

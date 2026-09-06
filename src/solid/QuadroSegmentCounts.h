#pragma once
#include "QuadroBoundary.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace quadro::detail {
// Solve compound-side equalities together. Sequential local increases can
// chase one another around a closed shell despite a feasible global solution.
inline bool BalanceSegmentCounts(std::vector<std::size_t>& counts,const SamplingOptions& options) {
    auto equations=options.equalSegmentSums;
    for(const auto& group:options.equalSegmentGroups)for(Id i=1;i<group.size();++i)equations.push_back({{group[0]},{group[i]}});
    const auto satisfies=[&](const auto& values) {
        for(const auto& equation:equations){std::size_t a=0,b=0;for(Id e:equation.first)a+=values[e];for(Id e:equation.second)b+=values[e];if(a!=b)return false;}
        return true;
    };
    if(satisfies(counts))return true;
    const Id n=counts.size();if(n>512||equations.size()>1024)return false;
    for(auto count:counts)if(count>options.maximumSegments)return false;
    std::vector<std::vector<double>> matrix;
    for(const auto& equation:equations){std::vector<double> row(n);for(Id e:equation.first)row[e]+=1;for(Id e:equation.second)row[e]-=1;matrix.push_back(std::move(row));}
    std::vector<Id> pivots;Id rank=0;
    for(Id column=0;column<n&&rank<matrix.size();++column) {
        Id pivot=rank;for(Id i=rank+1;i<matrix.size();++i)if(std::abs(matrix[i][column])>std::abs(matrix[pivot][column]))pivot=i;
        if(std::abs(matrix[pivot][column])<1.e-10)continue;
        std::swap(matrix[rank],matrix[pivot]);const double scale=matrix[rank][column];for(double& v:matrix[rank])v/=scale;
        for(Id i=0;i<matrix.size();++i)if(i!=rank){const double factor=matrix[i][column];if(std::abs(factor)>1.e-12)for(Id j=column;j<n;++j)matrix[i][j]-=factor*matrix[rank][j];}
        pivots.push_back(column);++rank;
    }
    std::vector<Id> free;
    for(Id i=0;i<n;++i)if(std::find(pivots.begin(),pivots.end(),i)==pivots.end())free.push_back(i);
    if(free.empty())return false;
    std::vector<std::vector<std::pair<Id,double>>> rows(n);
    for(Id j=0;j<free.size();++j)rows[free[j]].push_back({j,1});
    unsigned denominator=1;
    for(Id i=0;i<rank;++i)for(Id j=0;j<free.size();++j) {
        const double coefficient=-matrix[i][free[j]];if(std::abs(coefficient)<1.e-10)continue;
        unsigned d=1;for(;d<=16;++d)if(std::abs(coefficient-std::round(coefficient*d)/d)<1.e-8)break;
        if(d>16)return false;denominator=std::lcm(denominator,d);if(denominator>256)return false;
        rows[pivots[i]].push_back({j,std::round(coefficient*d)/d});
    }
    std::vector<double> values(free.size()),dual(n),norms(n),lower(n);
    for(Id i=0;i<n;++i){lower[i]=std::ceil(counts[i]/2.0);for(const auto& [j,c]:rows[i])norms[i]+=c*c;if(!(norms[i]>0))return false;}
    for(Id j=0;j<free.size();++j)values[j]=lower[free[j]];
    bool converged=false;
    // Hildreth projections onto the lower-bound halfspaces in nullspace
    // coordinates. Equality constraints remain exact throughout this solve.
    for(unsigned iteration=0;iteration<10000;++iteration) {
        for(Id i=0;i<n;++i) {
            double value=0;for(const auto& [j,c]:rows[i])value+=c*values[j];
            const double delta=std::max(-dual[i],(lower[i]-value)/norms[i]);dual[i]+=delta;
            for(const auto& [j,c]:rows[i])values[j]+=delta*c;
        }
        double violation=0;for(Id i=0;i<n;++i){double value=0;for(const auto& [j,c]:rows[i])value+=c*values[j];violation=std::max(violation,lower[i]-value);}
        if(violation<1.e-8){converged=true;break;}
    }
    if(!converged)return false;
    // Small rational coefficients permit an integer lift. Never accept a
    // rounded equality: verify every original equation and budget exactly.
    for(unsigned scale=1;scale<=8;++scale) {
        std::vector<double> integral(values.size());
        for(Id j=0;j<values.size();++j)integral[j]=denominator*std::round(values[j]*scale);
        std::vector<std::size_t> candidate(n);bool valid=true;
        for(Id i=0;i<n;++i) {
            double value=0;for(const auto& [j,c]:rows[i])value+=2*c*integral[j];
            if(!std::isfinite(value)||value<counts[i]-.01||value>options.maximumSegments+.01||std::abs(value-std::round(value))>1.e-6){valid=false;break;}
            candidate[i]=std::size_t(std::llround(value));
        }
        if(valid&&satisfies(candidate)){counts=std::move(candidate);return true;}
    }
    return false;
}
}

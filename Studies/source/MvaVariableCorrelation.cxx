/*! Study of correlation matrix and mutual information of BDT variables
This file is part of https://github.com/hh-italian-group/hh-bbtautau. */

#include "AnalysisTools/Run/include/program_main.h"
#include "h-tautau/Analysis/include/EventTuple.h"
#include "AnalysisTools/Core/include/exception.h"
#include "AnalysisTools/Core/include/AnalyzerData.h"
#include "TMVA/Factory.h"
#include "TMVA/DataLoader.h"
#include "TMVA/Tools.h"
#include "TMVA/TMVAGui.h"
#include "TMVA/DataSet.h"
#include "TMVA/DataSetInfo.h"
#include "TMVA/ClassInfo.h"
#include "TMVA/VariableTransformBase.h"
#include <TMatrixDEigen.h>
#include <TH1.h>
#include <TH2.h>
#include <fstream>
#include <random>
#include <algorithm>

struct Arguments { // list of all program arguments
    REQ_ARG(std::string, input_path);
    REQ_ARG(std::string, output_file);
    REQ_ARG(std::string, cfg_file);
    REQ_ARG(std::string, tree_name);
    REQ_ARG(Long64_t, number_events);
};

namespace analysis {

bool SM=false;

struct SampleEntry{
  std::string filename;
  double weight;
  bool issignal;
  SampleEntry() : weight(-1), issignal(false){}
};

std::ostream& operator<<(std::ostream& os, const SampleEntry& entry)
{
    os << entry.filename << " " << entry.weight << " " << std::boolalpha << entry.issignal;
    return os;
}

std::istream& operator>>(std::istream& is, SampleEntry& entry)
{
    is >> entry.filename >> entry.weight >> std::boolalpha >> entry.issignal;
    return is;
}

class MvaVariables{
private:
    std::map<std::string, std::map<std::string, std::vector<double>>> all_variables;
    std::map<std::string, double> variables;

public:
    MvaVariables(){}

    double& operator[](const std::string& name)
    {
        return variables[name];
    }

    void AddEvent(const std::string& sample)
    {
        std::map<std::string, std::vector<double>>& sample_vars = all_variables[sample];
        for(const auto& name_value : variables) {
            const std::string& name = name_value.first;
            const double value = name_value.second;
            sample_vars[name].push_back(value);
        }
    }
    const std::map<std::string, std::vector<double>>& GetSampleVariables(const std::string& sample) const
    {
        return all_variables.at(sample);
    }
};

using VarPair = std::pair<std::string,std::string>;

/*Create a pair of histogram for each variable. The first one is for signal, the second for background.
They are binned in the same way, to have at least 10 entries for bin.*/
std::map<std::string, std::pair<TH1D*, TH1D*>> GetHistos(std::map<std::string, std::vector<double>> sample_signal, std::map<std::string, std::vector<double>> sample_bkg, std::shared_ptr<TFile> outfile){

    std::map<std::string, std::pair<TH1D*, TH1D*>> histos;
    for(const auto& var_1 : sample_signal) {
        std::vector<double> vector_s = var_1.second;
        std::vector<double> vector_b = sample_bkg[var_1.first];

        int min_s = *std::min_element(vector_s.begin(),vector_s.end());
        int max_s = *std::max_element(vector_s.begin(),vector_s.end());
        int max_b = *std::max_element(vector_b.begin(),vector_b.end());
        int min_b = *std::min_element(vector_b.begin(),vector_b.end());

        double max = 0, min = 0;
        if (vector_b[max_b] >= vector_s[max_s]) max = vector_b[max_b];
        else max = vector_s[max_s];
        if (vector_b[min_b] <= vector_s[min_s]) min = vector_b[min_b];
        else min = vector_s[min_s];

        const int nbin = 50;
        TH1D* h_s = new TH1D((var_1.first+"Signal").c_str(),(var_1.first+"Signal").c_str(),nbin,min,max);
        h_s->SetCanExtend(TH1::kAllAxes);
        h_s->SetXTitle((var_1.first).c_str());
        for(Long64_t i = 0; i < vector_s.size(); i++){
            h_s->Fill(vector_s[i]);
        }
        root_ext::WriteObject(*h_s, outfile.get());
        int new_nbin_s = 0;
        double bin_s[nbin] = {};
        bin_s[0]= h_s->GetBinLowEdge(0);
        for (Long64_t i=0; i<=nbin; i++)
        {
            double entry = h_s->GetBinContent(i);
            Long64_t rebin = i;
            while (entry <= 10 && i<=nbin)
            {
                i++;
                entry = entry + h_s->GetBinContent(i);
            }
            if (entry > 10){
                new_nbin_s++;
                bin_s[new_nbin_s]=h_s->GetBinLowEdge(rebin);
            }
        }
        new_nbin_s++;
        bin_s[new_nbin_s]=h_s->GetBinLowEdge(nbin+1);
        h_s->Delete();

        histos[var_1.first].first = new TH1D((var_1.first+"Signal1").c_str(),(var_1.first+"Signal1").c_str(), new_nbin_s, bin_s);
        histos[var_1.first].first->SetXTitle((var_1.first).c_str());
        for(Long64_t i = 0; i < vector_s.size(); i++){
            histos[var_1.first].first->Fill(vector_s[i]);
        }
        histos[var_1.first].second = new TH1D((var_1.first+"Bkg1").c_str(),(var_1.first+"Bkg1").c_str(),new_nbin_s, bin_s);
        histos[var_1.first].second->SetXTitle((var_1.first).c_str());
        for(Long64_t i = 0; i < vector_b.size(); i++){
            histos[var_1.first].second->Fill(vector_b[i]);
        }
        root_ext::WriteObject(*histos[var_1.first].first, outfile.get());
        root_ext::WriteObject(*histos[var_1.first].second, outfile.get());

        int new_nbin_b = 0;
        double bin_b[nbin] = {};
        bin_b[0]= histos[var_1.first].second->GetBinLowEdge(0);
        for (Long64_t i=1; i<=new_nbin_s; i++)
        {
            double entry = histos[var_1.first].second->GetBinContent(i);
            Long64_t rebin = i;
            while (entry <= 10. && i<=new_nbin_s)
            {
                i++;
                entry = entry + histos[var_1.first].second->GetBinContent(i);
            }
            if (entry > 10){
                new_nbin_b++;
                bin_b[new_nbin_b]=histos[var_1.first].second->GetBinLowEdge(rebin);
            }
        }
        new_nbin_b++;
        bin_b[new_nbin_b]=histos[var_1.first].second->GetBinLowEdge(nbin+1);

        bool check = false;
        int count = 0;
        while(check==false && count<nbin){
            if (bin_s[count]!=bin_b[count]) check=true;
            count++;
        }
        if (check==true){
            histos[var_1.first].first->Delete();
            histos[var_1.first].second->Delete();
            histos[var_1.first].first = new TH1D((var_1.first+"Signal2").c_str(),(var_1.first+"Signal2").c_str(), new_nbin_b, bin_b);
            histos[var_1.first].first->SetXTitle((var_1.first).c_str());
            for(Long64_t i = 0; i < vector_s.size(); i++){
                histos[var_1.first].first->Fill(vector_s[i]);
            }
            histos[var_1.first].second = new TH1D((var_1.first+"Bkg2").c_str(),(var_1.first+"Bkg2").c_str(),new_nbin_b, bin_b);
            histos[var_1.first].second->SetXTitle((var_1.first).c_str());
            for(Long64_t i = 0; i < vector_b.size(); i++){
                histos[var_1.first].second->Fill(vector_b[i]);
            }
            histos[var_1.first].first->Scale(1/histos[var_1.first].first->Integral());
            histos[var_1.first].second->Scale(1/histos[var_1.first].second->Integral());
            root_ext::WriteObject(*histos[var_1.first].first, outfile.get());
            root_ext::WriteObject(*histos[var_1.first].second, outfile.get());
        }
        else{
            histos[var_1.first].first->Scale(1/histos[var_1.first].first->Integral());
            histos[var_1.first].second->Scale(1/histos[var_1.first].second->Integral());
            root_ext::WriteObject(*histos[var_1.first].first, outfile.get());
            root_ext::WriteObject(*histos[var_1.first].second, outfile.get());
        }
     }
     return histos;
}

//Chi2Test for signal vs background histo for each variable
std::map< std::string, double> Chi2Test(std::map<std::string, std::pair<TH1D*, TH1D*>> histos){

    std::map< std::string, double> pi_value;
    for(const auto& var_1 : histos) {
        std::cout<<"variabile: "<<var_1.first<<std::endl;
        double a = histos[var_1.first].first->Chi2Test(histos[var_1.first].second, "WWPCHI2/NDF");
        std::cout<<"chi2/ndof:"<<a<<std::endl<<std::endl;
        pi_value[var_1.first] = a;
    }
    return pi_value;
}

//Create covariance of two variables
double Covariance (const std::vector<double> vec_1, const std::vector<double> vec_2){
    double mean_1 = 0, mean_2 = 0;
    for (Long64_t i=0; i<vec_1.size(); i++ ){
        mean_1 = mean_1 + vec_1[i];
        mean_2 = mean_2 + vec_2[i];
    }
    mean_1 = mean_1/vec_1.size();
    mean_2 = mean_2/vec_1.size();
    double cov = 0;
    for (Long64_t i=0; i<vec_1.size(); i++ ){
        cov = cov + (vec_1[i] - mean_1) * (vec_2[i] - mean_2);
    }
    return cov/(vec_1.size()-1);
}

//Create correlation of two variables from covariance
std::map<VarPair, double> CovToCorr(std::map<VarPair, double> cov_matrix){
    std::map<VarPair, double> corr;
    for(const auto& elements : cov_matrix) {
        auto name_1 = elements.first;
        const VarPair var_11(name_1.first, name_1.first);
        double sigma_1 = std::sqrt(cov_matrix[var_11]);
        const VarPair var_22(name_1.second, name_1.second);
        double sigma_2 = std::sqrt(cov_matrix[var_22]);
        const VarPair var_12(name_1.first, name_1.second);
        corr[var_12] = cov_matrix[var_12]/(sigma_1*sigma_2);
    }
    return corr;
}

//Create covariance(correlation) histo matrix
void CreateMatrixHistos(std::map<std::string, std::vector<double>> sample_vars, std::map<VarPair, double> element, std::shared_ptr<TFile> outfile, std::string type,  std::string class_sample){
    int bin =  sample_vars.size();
    TH2D* matrix= new TH2D((type+"_"+class_sample).c_str(),(type+"_"+class_sample).c_str(),bin,0,0,bin,0,0);
    matrix->SetCanExtend(TH1::kAllAxes);
    matrix->SetBins(bin, 0, bin, bin, 0, bin);
    int i = 1;
    for(const auto& var_1 : sample_vars) {
        int j = 1;
        matrix->GetXaxis()->SetBinLabel(i, (var_1.first).c_str());
        matrix->GetYaxis()->SetBinLabel(i, (var_1.first).c_str());
        for(const auto& var_2 : sample_vars) {
            if (j>=i){
            const VarPair var_12(var_1.first, var_2.first);
            matrix->SetBinContent(i, j, element[var_12]);
            matrix->SetBinContent(j, i, element[var_12]);
            }
            j++;
        }
        i++;
    }
    root_ext::WriteObject(*matrix, outfile.get());
}

//Remove diagonal and under diagonal elements from a matrix
std::map<VarPair, double> RemoveDiagonal(std::map<VarPair, double> matrix){
    std::map<VarPair, double> matrix_diagonal;
    matrix_diagonal = matrix;
    for(const auto& elements : matrix) {
        auto name = elements.first;
        const VarPair var_11(name.first, name.first);
        matrix_diagonal.erase(var_11);
    }
    return matrix_diagonal;
}

//Difference between two vectors of pairs
std::vector< std::pair<VarPair, double>> Difference(std::vector< std::pair<VarPair, double>> vec_1, std::vector< std::pair<VarPair, double>> vec_2){
    std::vector< std::pair<VarPair, double>> diff;
    for(Long64_t i = 0; i < vec_1.size(); i++){
        std::pair<VarPair, double> element_1 = vec_1[i];
        std::pair<VarPair, double> element_2 = vec_2[i];
        if (element_1.first == element_2.first){
            VarPair nome=element_1.first;
            VarPair nome2=element_2.first;
            double difference = std::abs(element_1.second - element_2.second);
            diff.push_back(std::make_pair(element_1.first,difference));
        }
    }
    return diff;
}

//Kolmogorov test return the maximum distance
std::map< std::string, double> KolmogorovTest(std::map<std::string, std::vector<double>> sample_signal, std::map<std::string, std::vector<double>> sample_bkg){
    std::map< std::string, double> kolmogorov;
    double k;
    for(const auto& var_1 : sample_signal) {
        std::vector<double> vector_signal = sample_signal[var_1.first];
        std::sort(vector_signal.begin(), vector_signal.end());
        std::vector<double> vector_bkg = sample_bkg[var_1.first];
        std::sort(vector_bkg.begin(), vector_bkg.end());
        Double_t v_s[vector_signal.size()], v_b[vector_bkg.size()];
        for(Long64_t i = 0; i < vector_signal.size(); i++){
            v_s[i]=vector_signal[i];
            v_b[i]=vector_bkg[i];
        }
        k = TMath::KolmogorovTest(vector_signal.size(), v_s, vector_bkg.size(), v_b, "M");
        kolmogorov[var_1.first] = k;
    }
    return kolmogorov;
}

template<typename LVector1, typename LVector2, typename LVector3>
double Pzeta(const LVector1& l1_p4, const LVector2& l2_p4, const LVector3& met_p4)
{
    const auto ll_p4 = l1_p4 + l2_p4;
    const TVector2 ll_p2(ll_p4.Px(), ll_p4.Py());
    const TVector2 met_p2(met_p4.Px(), met_p4.Py());
    const TVector2 l1_u(std::cos(l1_p4.Phi()), std::sin(l1_p4.Phi()));
    const TVector2 l2_u(std::cos(l2_p4.Phi()), std::sin(l2_p4.Phi()));
    const TVector2 ll_u = l1_u + l2_u;
    const double ll_u_met = (met_p2+ll_p2) * ll_u;
    const double ll_mod = ll_u.Mod();
    return ll_u_met / ll_mod;
}
template<typename LVector1, typename LVector2>
double Pzetavisible(const LVector1& l1_p4, const LVector2& l2_p4)
{
    const auto ll_p4 = l1_p4 + l2_p4;
    const TVector2 ll_p2(ll_p4.Px(), ll_p4.Py());
    const TVector2 l1_u(std::cos(l1_p4.Phi()), std::sin(l1_p4.Phi()));
    const TVector2 l2_u(std::cos(l2_p4.Phi()), std::sin(l2_p4.Phi()));
    const TVector2 ll_u = l1_u + l2_u;
    const double ll_p2u = ll_p2 * ll_u;
    const double ll_mod = ll_u.Mod();
    return ll_p2u / ll_mod;
}


using SampleEntryCollection = std::vector<SampleEntry>;



class MvaClassification {
public:
    using Event = ntuple::Event;
    using EventTuple = ntuple::EventTuple;

    static const std::set<std::string>& GetDisabledBranches()
    {
        static const std::set<std::string> DisabledBranches_read = {
            "dphi_mumet", "dphi_metsv", "dR_taumu", "mT1", "mT2", "dphi_bbmet", "dphi_bbsv", "dR_bb", "m_bb", "n_jets",
            "btag_weight", "ttbar_weight",  "PU_weight", "shape_denominator_weight", "trigger_accepts", "trigger_matches",
            "event.tauId_keys_1","event.tauId_keys_2","event.tauId_values_1","event.tauId_values_2"
        };
        return DisabledBranches_read;
    }

    static SampleEntryCollection ReadConfig(const std::string& cfg_file){
        std::ifstream f(cfg_file);
        SampleEntryCollection collection;
        while(f.good()){
            std::string line;
            std::getline(f, line);
            if (line.size()==0 || line.at(0)=='#') continue;
            std::istringstream s(line);
            SampleEntry entry;
            s>>entry;
            collection.push_back(entry);
        }
        return collection;
    }

    MvaClassification(const Arguments& _args): args(_args), samples(ReadConfig(args.cfg_file())),
        outfile(root_ext::CreateRootFile(args.output_file())), vars()
    {
    }

    void Run()
    {
        const double mass_top = 173.21;
        for(const SampleEntry& entry:samples)
        {
            auto input_file=root_ext::OpenRootFile(args.input_path()+"/"+entry.filename);
            EventTuple tuple(args.tree_name(), input_file.get(), true, GetDisabledBranches());
            std::cout<<entry<<" number of events: "<<std::min(tuple.GetEntries(),args.number_events())<<std::endl;
            std::string samplename = entry.issignal ? "Signal" : "Background";

            for(Long64_t current_entry = 0; current_entry < std::min(tuple.GetEntries(),args.number_events()); ++current_entry) {
                tuple.GetEntry(current_entry);
                const Event& event = tuple.data();

                if (event.eventEnergyScale!=0 || (event.q_1+event.q_2)!=0 || event.jets_p4.size() < 2
                    || event.extraelec_veto==true || event.extramuon_veto==true) continue;

                LorentzVectorE_Float bb= event.jets_p4[0] + event.jets_p4[1];
                LorentzVectorM_Float leptons= event.p4_1 + event.p4_2;
                LorentzVectorM_Float leptonsMET= event.p4_1 + event.p4_2 + event.pfMET_p4;

                double circular_cut=std::sqrt(pow(event.SVfit_p4.mass()-116.,2)+pow(bb.M()-111,2));
                if (circular_cut>40) continue;
//                if ((args.tree_name=="eTau" && circular_cut>40)||(args.tree_name=="muTau" && circular_cut>30)) continue;

                vars["pt_l1"] = event.p4_1.pt();
                vars["pt_l2"] = event.p4_2.pt();
//                vars["pt_jet1"] = event.jets_p4[0].pt();
//                vars["pt_jet2"] = event.jets_p4[1].pt();
//                vars["pt_l1l2"] = leptons.pt();
//                vars["pt_sv"] = event.SVfit_p4.pt();
//                vars["pt_l1l2met"] = leptonsMET.pt();
//                vars["pt_b1b2"] = bb.pt();
                vars["pt_met"] = event.pfMET_p4.pt();

//                vars["p_zeta"] = Pzeta(event.p4_1, event.p4_2, event.pfMET_p4);
//                vars["p_zeta_visible"] = Pzetavisible(event.p4_1,event.p4_2);

                vars["abs(dphi_l1l2)"] = std::abs(ROOT::Math::VectorUtil::DeltaPhi(event.p4_1, event.p4_2));
                vars["dphi_l1l2"] = (ROOT::Math::VectorUtil::DeltaPhi(event.p4_1, event.p4_2));
//                vars["abs(dphi_jet1jet2)"] = std::abs(ROOT::Math::VectorUtil::DeltaPhi(event.jets_p4[0], event.jets_p4[1]));
                vars["dphi_jet1jet2"] = (ROOT::Math::VectorUtil::DeltaPhi(event.jets_p4[0], event.jets_p4[1]));
//                vars["abs(dphi_l1met)"] = std::abs(ROOT::Math::VectorUtil::DeltaPhi(event.p4_1, event.pfMET_p4));
//                vars["dphi_l1met"] = (ROOT::Math::VectorUtil::DeltaPhi(event.p4_1, event.pfMET_p4));
//                vars["abs(dphi_l2met)"] = std::abs(ROOT::Math::VectorUtil::DeltaPhi(event.p4_2, event.pfMET_p4));
                vars["dphi_l2met"] = (ROOT::Math::VectorUtil::DeltaPhi(event.p4_2, event.pfMET_p4));
//                vars["abs(dphi_l1l2met)"] = std::abs(ROOT::Math::VectorUtil::DeltaPhi(leptons, event.pfMET_p4));
//                vars["dphi_l1l2met"] = (ROOT::Math::VectorUtil::DeltaPhi(leptons, event.pfMET_p4));
//                vars["abs(dphi_svmet)"] = std::abs(ROOT::Math::VectorUtil::DeltaPhi(event.SVfit_p4, event.pfMET_p4));
                vars["dphi_svmet"] = (ROOT::Math::VectorUtil::DeltaPhi(event.SVfit_p4, event.pfMET_p4));
//                vars["abs(dphi_b1b2met)"] = std::abs(ROOT::Math::VectorUtil::DeltaPhi(bb, event.pfMET_p4));
                vars["dphi_b1b2met"] = (ROOT::Math::VectorUtil::DeltaPhi(bb, event.pfMET_p4));
                vars["abs(dphi_b1b2sv)"] = std::abs(ROOT::Math::VectorUtil::DeltaPhi(bb, event.SVfit_p4));
                vars["dphi_b1b2sv"] = (ROOT::Math::VectorUtil::DeltaPhi(bb, event.SVfit_p4));

                vars["abs(deta_l1l2)"] = std::abs(event.p4_1.eta() - event.p4_2.eta());
                vars["deta_l1l2"] = (event.p4_1.eta() - event.p4_2.eta());
//                vars["abs(deta_jet1jet2)"] = std::abs(event.jets_p4[0].eta() - event.jets_p4[1].eta());
                vars["deta_jet1jet2"] = (event.jets_p4[0].eta() - event.jets_p4[1].eta());
//                vars["abs(deta_l1met)"] = std::abs(event.p4_1.eta()-event.pfMET_p4.eta());
//                vars["deta_l1met"] = (event.p4_1.eta()-event.pfMET_p4.eta());
//                vars["abs(deta_l2met)"] = std::abs(event.p4_2.eta()-event.pfMET_p4.eta());
//                vars["deta_l2met"] = (event.p4_2.eta()-event.pfMET_p4.eta());
//                vars["abs(deta_l1l2met)"] = std::abs(leptons.eta()-event.pfMET_p4.eta());
//                vars["deta_l1l2met"] = (leptons.eta()-event.pfMET_p4.eta());
//                vars["abs(deta_svmet)"] = std::abs(event.SVfit_p4.eta()-event.pfMET_p4.eta());
//                vars["deta_svmet"] = (event.SVfit_p4.eta()-event.pfMET_p4.eta());
//                vars["abs(deta_b1b2met)"] = std::abs(bb.eta()-event.pfMET_p4.eta());
//                vars["deta_b1b2met"] = (bb.eta()-event.pfMET_p4.eta());
                vars["abs(deta_b1b2sv)"] = std::abs(bb.eta()-event.SVfit_p4.eta());
                vars["deta_b1b2sv"] = bb.eta()-event.SVfit_p4.eta();

//                vars["dR_l1l2"] = (ROOT::Math::VectorUtil::DeltaR(event.p4_1, event.p4_2));
                vars["dR_b1b2"] = (ROOT::Math::VectorUtil::DeltaR(event.jets_p4[0], event.jets_p4[1]));
                vars["dR_l1met"] = (ROOT::Math::VectorUtil::DeltaR(event.p4_1, event.pfMET_p4));
                vars["dR_l2met"] = (ROOT::Math::VectorUtil::DeltaR(event.p4_2, event.pfMET_p4));
                vars["dR_l1l2met"] = (ROOT::Math::VectorUtil::DeltaR(leptons, event.pfMET_p4));
                vars["dR_svmet"] = (ROOT::Math::VectorUtil::DeltaR(event.SVfit_p4, event.pfMET_p4));
//                vars["dR_b1b2met"] = (ROOT::Math::VectorUtil::DeltaR(bb, event.pfMET_p4));
//                vars["dR_b1b2sv"] = (ROOT::Math::VectorUtil::DeltaR(bb, event.SVfit_p4));

//                vars["dR_jet1jet2Pt_b1b2"] = (ROOT::Math::VectorUtil::DeltaR(event.jets_p4[0], event.jets_p4[1]))*bb.Pt();
//                vars["dR_l1l2Pt_sv"] = (ROOT::Math::VectorUtil::DeltaR(event.p4_1, event.p4_2))*event.SVfit_p4.Pt();

//                vars["massinvariant_l1l2met"] = ROOT::Math::VectorUtil::InvariantMass(leptons,event.pfMET_p4); //
                vars["mass_sv"] = event.SVfit_p4.M();
//                vars["mass_l1l2"] = std::sqrt(pow(event.p4_1.Et()+event.p4_2.Et(),2)-pow(event.p4_1.px()+event.p4_2.px(),2)+pow(event.p4_1.py()+event.p4_2.py(),2));//
                vars["mass_b1b2"] = bb.M();
//                vars["MT_l1"] = Calculate_MT(event.p4_1,event.pfMET_p4);
                vars["MT_l2"] = Calculate_MT(event.p4_2,event.pfMET_p4);
                vars["MT_sv"] = Calculate_MT(event.SVfit_p4, event.pfMET_p4); //
                vars["MT_l1l2"] = Calculate_MT(leptons, event.pfMET_p4);
                vars["mass_H"] = ROOT::Math::VectorUtil::InvariantMass(bb,event.SVfit_p4);
                vars["mass_H(l1l2met+b1b2)"] = ROOT::Math::VectorUtil::InvariantMass(bb,leptonsMET);

                LorentzVectorM_Float t1 = event.p4_1 + event.jets_p4[0] + event.pfMET_p4;
                LorentzVectorM_Float t2 = event.p4_2 + event.jets_p4[0] + event.pfMET_p4;
                LorentzVectorM_Float t3 = event.p4_1 + event.jets_p4[1] + event.pfMET_p4;
                LorentzVectorM_Float t4 = event.p4_2 + event.jets_p4[1] + event.pfMET_p4;
                double d1 = std::abs(t1.mass() - mass_top);
                double d2 = std::abs(t2.mass() - mass_top);
                double d3 = std::abs(t3.mass() - mass_top);
                double d4 = std::abs(t4.mass() - mass_top);
                if (d1<d2 && d1<d3 && d1<d4) vars["mass_top"] = d1;
                if (d2<d1 && d2<d3 && d2<d4) vars["mass_top"] = d2;
                if (d3<d1 && d3<d2 && d3<d4) vars["mass_top"] = d3;
                if (d4<d1 && d4<d3 && d4<d2) vars["mass_top"] = d4;

                TLorentzVector lvec_t1(event.p4_1.x(),event.p4_1.y(),event.p4_1.z(),event.p4_1.t());
                TLorentzVector lvec_t2(event.p4_2.x(),event.p4_2.y(),event.p4_2.z(),event.p4_2.t());
                TLorentzVector lvec_svfit(event.SVfit_p4.x(),event.SVfit_p4.y(),event.SVfit_p4.z(),event.SVfit_p4.t());
                TVector3 boost_1 = -(lvec_t1+lvec_t2).BoostVector();
                lvec_t1.Boost(boost_1);
                lvec_svfit.Boost(boost_1);
//                vars["theta_1"] = ROOT::Math::VectorUtil::Angle(lvec_t1,lvec_svfit);

                TLorentzVector lvec_j1(event.jets_p4[0].x(),event.jets_p4[0].y(),event.jets_p4[0].z(),event.jets_p4[0].t());
                TLorentzVector lvec_j2(event.jets_p4[1].x(),event.jets_p4[1].y(),event.jets_p4[1].z(),event.jets_p4[1].t());
                TLorentzVector lvec_j1j2(bb.x(),bb.y(),bb.z(),bb.t());
                TVector3 boost_2 = -(lvec_j1+lvec_j2).BoostVector();
                lvec_j1.Boost(boost_2);
                lvec_j1j2.Boost(boost_2);
                vars["theta_2"] = ROOT::Math::VectorUtil::Angle(lvec_j1,lvec_j1j2);

                TLorentzVector lvec_sv(event.SVfit_p4.x(),event.SVfit_p4.y(),event.SVfit_p4.z(),event.SVfit_p4.t());
                TLorentzVector lvec_b1b2(bb.x(),bb.y(),bb.z(),bb.t());
                TLorentzVector lvec_l1(event.p4_1.x(),event.p4_1.y(),event.p4_1.z(),event.p4_1.t());
                TLorentzVector lvec_b1(event.jets_p4[0].x(),event.jets_p4[0].y(),event.jets_p4[0].z(),event.jets_p4[0].t());
                TVector3 boost = (lvec_sv+lvec_b1b2).BoostVector();
                lvec_l1.Boost(boost);
                lvec_t2.Boost(boost);
                lvec_b1.Boost(boost);
                lvec_j2.Boost(boost);
                TVector3 vec_l1 = lvec_l1.Vect();
                TVector3 vec_b1 = lvec_b1.Vect();
                TVector3 vec_l2 = lvec_t2.Vect();
                TVector3 vec_b2 = lvec_j2.Vect();
                TVector3 n1, n2;
                n1 = vec_l1.Cross(vec_l2);
                n2 = vec_b1.Cross(vec_b2);
                vars["phi"] = ROOT::Math::VectorUtil::Angle(n1,n2);

                lvec_sv.Boost(boost);
                vars["theta_star"] = std::atan(std::sqrt(pow(lvec_sv.X(),2)+pow(lvec_sv.Y(),2))/lvec_sv.Z());

                TLorentzVector lz_axis(0,0,1,0);
                lz_axis.Boost(boost);
                TVector3 n_1;
                TVector3 z_axis = lz_axis.Vect();
                TVector3 vec_sv = lvec_sv.Vect();
                n_1 = z_axis.Cross(vec_sv);
                vars["phi_1"] = ROOT::Math::VectorUtil::Angle(n1,n_1);

                vars.AddEvent(samplename);
            }
        }

        std::map<std::string, std::vector<double>> sample_vars_signal = vars.GetSampleVariables("Signal");
        std::map<std::string, std::vector<double>> sample_vars_bkg = vars.GetSampleVariables("Background");
        std::cout<<"n.variabili: "<<sample_vars_signal.size()<<std::endl;

        std::map<std::string, std::pair<TH1D*, TH1D*>> histos = GetHistos(sample_vars_signal, sample_vars_bkg, outfile);

        std::map<VarPair, double> cov_matrix_signal, cov_matrix_bkg;
        for(const auto& var_1 : sample_vars_signal) {
            for(const auto& var_2 : sample_vars_signal) {
                const VarPair var_12(var_1.first, var_2.first);
                const VarPair var_21(var_2.first, var_1.first);
                if(cov_matrix_signal.count(var_21)) continue;
                const double cov = Covariance(var_1.second, var_2.second);
                cov_matrix_signal[var_12] = cov;
            }
        }

        for(const auto& var_1 : sample_vars_bkg) {
            for(const auto& var_2 : sample_vars_bkg) {
                const VarPair var_12(var_1.first, var_2.first);
                const VarPair var_21(var_2.first, var_1.first);
                if(cov_matrix_bkg.count(var_21)) continue;
                const double cov = Covariance(var_1.second, var_2.second);
                cov_matrix_bkg[var_12] = cov;
            }
        }

//        CreateMatrixHistos(sample_vars_signal,cov_matrix_signal,anaData,"covariance","Signal");
//        CreateMatrixHistos(sample_vars_bkg,cov_matrix_bkg,anaData,"covariance","Background");

        std::map<VarPair, double> corr_matrix_signal, corr_matrix_bkg;
        corr_matrix_signal = CovToCorr(cov_matrix_signal);
        corr_matrix_bkg = CovToCorr(cov_matrix_bkg);

        CreateMatrixHistos(sample_vars_signal,corr_matrix_signal,outfile,"correlation","Signal");
        CreateMatrixHistos(sample_vars_bkg,corr_matrix_bkg,outfile,"correlation","Background");

        std::map<VarPair, double> corr_matrix_signal_fix, corr_matrix_bkg_fix;
        corr_matrix_signal_fix = RemoveDiagonal(corr_matrix_signal);
        corr_matrix_bkg_fix = RemoveDiagonal(corr_matrix_bkg);

        std::vector< std::pair<VarPair, double>> corr_vector_signal(corr_matrix_signal_fix.begin(), corr_matrix_signal_fix.end());
        std::vector< std::pair<VarPair, double>> corr_vector_bkg(corr_matrix_bkg_fix.begin(), corr_matrix_bkg_fix.end());
        std::vector< std::pair<VarPair, double>> corr_vector_difference = Difference(corr_vector_signal,corr_vector_bkg);
        std::map< std::string, double> kolmogorov = KolmogorovTest(sample_vars_signal, sample_vars_bkg);
        std::map< std::string, double> chi2 = Chi2Test(histos);

        std::sort(corr_vector_difference.begin(), corr_vector_difference.end(), [](const std::pair<VarPair, double>& a, const std::pair<VarPair, double>& b) {
            return std::abs(a.second) < std::abs(b.second);
        });

        std::map<std::string, int> eliminate;
        std::ofstream ofs(args.tree_name()+".txt", std::ofstream::out);
        ofs << "Variable    " << "  Variable2   " << "  corr_s  " << "  corr_b    " << "  corr_d    "<<"  chi2_1    "<<"  chi2_2    "<<"  KS_1    "<<"  KS_2"<<std::endl;
        for(Long64_t i = 0; i < corr_vector_difference.size(); i++){
            std::pair<VarPair, double> element_difference = corr_vector_difference[i];
            VarPair pair = element_difference.first;

            if ((std::abs(corr_matrix_signal_fix[pair])>0.3 && std::abs(corr_matrix_bkg_fix[pair])>0.3) && element_difference.second<0.6){
                ofs <<pair.first<<"    "<<pair.second<<"    "<<std::abs(corr_matrix_signal_fix[pair])<<"    "<<std::abs(corr_matrix_bkg_fix[pair])<<"    "<<element_difference.second<<"   "<<chi2[pair.first]<<"    "<<chi2[pair.second]<<"  "<<kolmogorov[pair.first]<<"    "<<kolmogorov[pair.second]<<std::endl;
                if (kolmogorov[pair.first]<=kolmogorov[pair.second] && chi2[pair.first]<=chi2[pair.second]) eliminate[pair.first]++;
                if (kolmogorov[pair.first]>kolmogorov[pair.second] && chi2[pair.first]>chi2[pair.second]) eliminate[pair.second]++;
            }
        }
        ofs.close();
        std::ofstream off("elimination.txt", std::ofstream::out);
        for(const auto& i : eliminate){
            off << i.first << "     " << i.second<<std::endl;
        }
        off.close();
    }
private:
    Arguments args;
    SampleEntryCollection samples;
    std::shared_ptr<TFile> outfile;
    MvaVariables vars;
};


}


PROGRAM_MAIN(analysis::MvaClassification, Arguments) // definition of the main program function

//dies/config/mva_config.cfg --tree_name eTau --number_events 1000000









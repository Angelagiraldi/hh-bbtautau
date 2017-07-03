# How to run
### VariableDistribution
```shell
./run.sh VariableDistribution --input_path ~/Desktop/tuples --output_file VariableDistribution_muTau.root --cfg_file hh-bbtautau/Studies/config/mva_config.cfg --tree_name muTau
```
### MvaRangesCompatibility
```shell
./run.sh MvaRangesCompatibility --input_path ~/Desktop/tuples --output_file MassVariables__muTau_2.root --cfg_file hh-bbtautau/Studies/config/mva_config.cfg --tree_name muTau  --number_threads 4 --number_variables 20 --number_events 10000 --number_sets 3 --set 2
```
### MvaVariableSelection
```shell
./run.sh MvaVariableSelection --input_path ~/Desktop/tuples --output_file MassVariables__muTau_2.root --cfg_file hh-bbtautau/Studies/config/mva_config.cfg --tree_name muTau  --number_threads 4 --number_variables 20 --number_events 10000 --number_sets 3 --set 2
```
### MvaTraining
```shell
./run.sh MvaTraining --input_path ~/Desktop/tuples --output_file prova --cfg_file hh-bbtautau/Studies/config/mva_config.cfg  --number_events 20000 --range Low --seed 123
```
### MvaAnalyzer
```shell
./run.sh MvaAnalyzer --input_file BDT_lm_2345678.root --output_file analyzer_2345678.root
```
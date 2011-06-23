/**
 * Main feature list
 * 1. support various annotion
 *    UPSTREAM / DOWNSTREAM 
 *    5PRIME_UTR / 3PRIME_UTR
 *    INTRON
 *    SYNONYMOUS / NONSYNONYMOUS
 *    STOP_GAIN / STOP_LOST
 *    START_GAIN / START_LOST ??
 *    SPLICE_SITE
 *    INTROGENETIC
 *    CODON_DELETION / CODON_INSERTION / FRAME_SHIFT
 *    
 * 2. input format are flexible
 *    support VCF, as well as user specified.
 * 3. output format are configurable.
 *    user specify
 * 4. output basis statisitcs:
 *    freq of each annotation  .anno.freq 
 *    freq of each base change .base.freq
 *    freq of codon change     .codon.freq
 *    freq of indel length     .indel.freq
 */

/**
 * Example, 1-index range 3-4, inclusive on the boundary
 * in UCSC setting, this range is coded as 2-4 (see below)
 *             @
 * 0-base: 0 1 2 3 4 5 
 * 1-base: 1 2 3 4 5 6
 *               ^
 * the tricky thing is 0-length range, in such case,
 * the UCSC coded the start and end as the same value
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <string>
#include <vector>
#include <map>

#include "Argument.h"
#include "IO.h"
#include "StringUtil.h"

#include "Gene.h"
// class Chromosome{
// public:
//     char operator[] (int i) {
//     };
//     int size() const {
//         return ;
//     }
// private:
    
// };
class GenomeSequence{
public:
    /**
     * @return true: if loads successful
     */
    bool open(const char* fileName){
        LineReader lr(fileName);
        std::string line;
        std::string chr = "NoName";
        while(lr.readLine(&line) > 0) {
            if (line.size() > 0) {
                if (line[0] == '>') {
                    // new chromosome
                    unsigned int idx = line.find_first_of(' ');
                    chr = line.substr(1, idx - 1);
                    this->data[chr] = "";
                } else {
                    stringStrip(&line);
                    this->data[chr] += line;
                }
            }
        }
        return true;
    };
    int size() const {
        return this->data.size();
    };
    std::string& getChromosome(const char* c){
        return this->data[c];
    };
    const std::string& operator[] (const std::string& c) {
        return this->data[c];
    }
    bool exists(const std::string& c){
        if (this->data.find(c) != this->data.end())
         return true;
        return false;
    }
public:
    std::map<std::string, std::string> data;
};

typedef enum AnnotationType{
    UPSTREAM = 0,
    DOWNSTREAM,
    UTR5,
    UTR3,
    INTRON,
    EXON,
    SNV,                    /*SNV contain the following 6 types, it appears when there is no reference.*/
    SYNONYMOUS,
    NONSYNONYMOUS,
    STOP_GAIN,
    STOP_LOST,
    START_GAIN,
    START_LOSE,
    NORMAL_SPLICE_SITE,
    ESSENTIAL_SPLICE_SITE,
    INTROGENIC
} AnnotationType;

char AnnotationString[][32]= {
    "Upstream", 
    "Downstream",
    "Utr5",
    "Utr3",
    "Intron",
    "Exon",
    "SNV",
    "Synonymous",
    "Nonsynonymous",
    "Stop_Gain",
    "Stop_Lost",
    "Start_Gain",
    "Start_Lose",
    "Normal_Splice_Site",
    "Essential_Splice_Site",
    "Introgenic"
};
struct GeneAnnotationParam{
    GeneAnnotationParam():
        upstreamRange(500),
        downstreamRange(500),
        spliceIntoExon(3),
        spliceIntoIntron(8) {};
    int upstreamRange;      // upstream def
    int downstreamRange;    // downstream def
    int spliceIntoExon;     // essential splice site def
    int spliceIntoIntron;   // essentail splice site def
};

class GeneAnnotation{
public:
    void readGeneFile(const char* geneFileName){
        std::string line;
        std::vector<std::string> fields;
        LineReader lr(geneFileName);
        while (lr.readLine(&line) > 0) {
            stringStrip(&line);
            if (line.size()>0 && line[0] == '#' || line.size() == 0) continue; // skip headers and empty lines
            Gene g;
            g.readLine(line.c_str());
            this->geneList[g.chr].push_back(g);
        }
        return;
    }; 
    void openReferenceGenome(const char* referenceGenomeFileName) {
        this->gs.open(referenceGenomeFileName);
        return;
/*
        std::string line;
        std::vector<std::string> fields;
        LineReader lr(geneFileName);

        // check if -bs.umfa file exists
        std::string umfaFileName = referenceGenomeFileName;
        stringSlice(&umfaFileName, 0, -3);
        umfaFileName += "-bs.umfa";
        FILE* fp = fopen(umfaFileName.c_str(), "r");
        this->gs.setReferenceName(referenceGenomeFileName);
        if (fp == NULL) { // does not exist binary format, so try to create one
            fprintf(stdout, "Create binary reference genome file for the first run\n");
            this->gs.create();
        } else{
            fclose(fp);
        }
        if (this->gs.open()) {
            fprintf(stderr, "Cannot open reference genome file %s\n", referenceGenomeFileName);
            exit(1);
        }
        if (!convert2Chromosome(&this->gs, &this->reference)) {
            fprintf(stderr, "Cannot use GenomeSequence by Chromosome.\n");
            exit(1);
        }
*/
    };
    // we take a VCF input file for now
    void annotate(const char* inputFileName, const char* outputFileName){
        std::string annotationString;
        LineReader lr(inputFileName);
        std::vector<std::string> field;
        while (lr.readLineBySep(&field, "\t") > 0) {
            if (field.size() < 4) continue;
            std::string chr = field[0];
            int pos = toInt(field[1]);
            std::string ref = field[3];
            std::string alt = field[4];
            int geneBegin;
            int geneEnd;
            this->findInRangeGene(&geneBegin, &geneEnd, field[0], &pos);
            if (geneEnd < 0) continue;
            annotationString.clear();
            for (unsigned int i = geneBegin; i <= geneEnd; i++) {
                this->annotateByGene(i, chr, pos, ref, alt, &annotationString);
                printf("%s %d ref=%s alt=%s has annotation: %s\n",
                       chr.c_str(), pos, 
                       ref.c_str(), alt.c_str(),
                       annotationString.c_str());
            }
        }
        return;
    };
    void setAnnotationParameter(GeneAnnotationParam& param) {
        this->param = param;
    };
private:
    // store results in start and end, index are 0-based, inclusive
    // find gene whose range plus downstream/upstream overlaps chr:pos 
    void findInRangeGene(int* start, int* end, const std::string& chr, int* pos) {
        *start = -1;
        *end = -1;
        std::vector<Gene>& g = this->geneList[chr];
        if (g.size() == 0) {
            return;
        } 
        unsigned int gLen = g.size();
        for (unsigned int i = 0; i < gLen ; i++) {
            if (g[i].forwardStrand) { // forward strand
                if (g[i].tx.start - param.upstreamRange < (*pos) && (*pos) < g[i].tx.end + param.downstreamRange) {
                    if (*start < 0) {
                        *start = i;
                    }
                    *end = i;
                }
            } else { // reverse strand
                if (g[i].tx.start - param.downstreamRange < *pos && *pos < g[i].tx.end + param.upstreamRange) {
                    if (*start < 0) {
                        *start = i;
                    }
                    *end = i;
                }
            }
        }
        // printf("start = %d, end = %d \n", *start, *end);
        return;
    };
    
    void annotateByGene(int geneIdx, const std::string& chr, const int& variantPos, const std::string& ref, const std::string& alt,
                        std::string* annotationString){
        Gene& g = this->geneList[chr][geneIdx];
        std::vector<std::string> annotation;
        int dist2Gene;
        int utrPos, utrLen;
        int exonNum; // which exon
        int codonNum; // which codon
        int codonPos[3] = {0, 0, 0}; // the codon position
        std::string codonRef;
        std::string codonAlt;
        AnnotationType type; // could be one of 
                             //    SYNONYMOUS / NONSYNONYMOUS
                             //    STOP_GAIN / STOP_LOST
                             //    START_GAIN / START_LOST ??
        int intronNum; // which intron
        bool isEssentialSpliceSite;
        if (g.isUpstream(variantPos, param.upstreamRange, &dist2Gene)) {
            annotation.push_back(AnnotationString[DOWNSTREAM]);
        } else if (g.isDownstream(variantPos, param.upstreamRange, &dist2Gene)) {
            annotation.push_back(AnnotationString[DOWNSTREAM]);
        } else if (g.isExon(variantPos, &exonNum)){//, &codonNum, codonPos)) {
            annotation.push_back(AnnotationString[EXON]);
            if (!g.isNonCoding()) {
                if (g.is5PrimeUtr(variantPos, &utrPos, &utrLen)) {
                    annotation.push_back(AnnotationString[UTR5]);
                } else if (g.is3PrimeUtr(variantPos, &utrPos, &utrLen)) {
                    annotation.push_back(AnnotationString[UTR3]);
                } else { // cds part has base change
                    if (g.calculatCodon(variantPos, &codonNum, codonPos)) {
                        // let's check which base, and how they are changed
                        std::string s("pos="); 
                        s += toString(codonPos[0]);
                        s += toString(codonPos[1]);
                        s += toString(codonPos[2]);
                        if (this->gs.size()>0 && 
                            this->gs.exists(g.chr)){
                            s.push_back(this->gs[g.chr][codonPos[0]-1]);
                            s.push_back(this->gs[g.chr][codonPos[1]-1]);
                            s.push_back(this->gs[g.chr][codonPos[2]-1]);
                         }
                        annotation.push_back(s);
                    } else {
                        annotation.push_back(AnnotationString[SNV]);
                    }
                }
            }
            // check splice site
            if (g.isSpliceSite(variantPos, param.spliceIntoExon, param.spliceIntoIntron, &isEssentialSpliceSite)){
                if (isEssentialSpliceSite)
                    annotation.push_back(AnnotationString[ESSENTIAL_SPLICE_SITE]);
                else 
                    annotation.push_back(AnnotationString[NORMAL_SPLICE_SITE]);
            }
        } else if (g.isIntron(variantPos, &intronNum)) {
            annotation.push_back(AnnotationString[INTRON]);
            // check splice site
            if (g.isSpliceSite(variantPos, param.spliceIntoExon, param.spliceIntoIntron, &isEssentialSpliceSite)){
                if (isEssentialSpliceSite)
                    annotation.push_back(AnnotationString[ESSENTIAL_SPLICE_SITE]);
                else 
                    annotation.push_back(AnnotationString[NORMAL_SPLICE_SITE]);
            }
        } else {
            annotation.push_back("Intergenic");
        }
        annotationString->assign(stringJoin(annotation, ":"));
        return;
    };
    /**@return -1: unknow type
     *          0: single mutation
     *          1: deletion
     *          2: insertion
     */
    int getMutationType(const char* ref, const char* alt) {
        unsigned int refLen = strlen(ref);
        unsigned int altLen = strlen(alt);
        if (refLen == altLen) 
            return 0;
        else if (refLen > altLen) 
            return 1;
        else if (refLen < altLen)
            return 2;
        return -1;
    };
    GeneAnnotationParam param;
    std::map <std::string, std::vector<Gene> > geneList;
    std::string annotation;
    GenomeSequence gs;
};
int main(int argc, char *argv[])
{

    BEGIN_PARAMETER_LIST(pl)
        ADD_STRING_PARAMETER(pl, inputFile, "-i", "Specify input VCF file")
        ADD_STRING_PARAMETER(pl, geneFile, "-g", "Specify UCSC refFlat gene file")
        ADD_STRING_PARAMETER(pl, referenceFile, "-r", "Specify reference genome position")
    END_PARAMETER_LIST(pl)
        ;
    
    pl.Read(argc, argv);
    pl.Status();
    
    GeneAnnotation ga;
    ga.readGeneFile("test.gene.txt");
    ga.openReferenceGenome("test.fa");
    ga.annotate("test.vcf", "test.output.vcf");

    return 0;
}
